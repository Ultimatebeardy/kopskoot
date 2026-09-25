// SPDX-License-Identifier: GPL-2.0-or-later
#include "killsense/profile.hpp"

#include <cctype>

#include "killsense/fsutil.hpp"
#include "killsense/logging.hpp"
#include "killsense/version.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <regex>
#include <set>

namespace killsense {

using json = nlohmann::json;

const char *detectorKindId(DetectorKind kind)
{
	switch (kind) {
	case DetectorKind::Template:
		return "template";
	case DetectorKind::Ocr:
		return "ocr";
	case DetectorKind::Color:
		return "color";
	case DetectorKind::HudChange:
		return "hud_change";
	case DetectorKind::Onnx:
		return "onnx";
	}
	return "unknown";
}

const RegionSpec *GameProfile::findRegion(const std::string &rid) const
{
	for (const auto &r : regions)
		if (r.id == rid)
			return &r;
	return nullptr;
}

const DetectorSpec *GameProfile::findDetector(const std::string &did) const
{
	for (const auto &d : detectors)
		if (d.id == did)
			return &d;
	return nullptr;
}

std::map<std::string, NormRect> GameProfile::resolveRegions(double sourceAspect) const
{
	std::map<std::string, NormRect> out;
	for (const auto &r : regions)
		out[r.id] = r.rect;
	if (sourceAspect <= 0.0)
		return out;
	for (const auto &ov : aspectOverrides) {
		if (ov.aspect > 0.0 && std::fabs(ov.aspect - sourceAspect) / ov.aspect <= 0.02) {
			for (const auto &kv : ov.regions)
				out[kv.first] = kv.second;
			break;
		}
	}
	return out;
}

namespace {

// Small validating reader that records errors instead of throwing.
class Reader {
public:
	std::vector<std::string> errors;
	std::vector<std::string> warnings;

	void checkKeys(const json &obj, const std::string &path, std::initializer_list<const char *> allowed)
	{
		if (!obj.is_object())
			return;
		std::set<std::string> allow;
		for (const char *k : allowed)
			allow.insert(k);
		for (auto it = obj.begin(); it != obj.end(); ++it) {
			if (!allow.count(it.key()) && it.key().rfind("_", 0) != 0) // "_comment" style keys allowed
				warnings.push_back(path + ": unknown key '" + it.key() + "' ignored");
		}
	}

	std::string str(const json &obj, const char *key, const std::string &path, const std::string &def,
			bool required = false)
	{
		if (!obj.contains(key)) {
			if (required)
				errors.push_back(path + "." + key + " is required");
			return def;
		}
		const auto &v = obj.at(key);
		if (!v.is_string()) {
			errors.push_back(path + "." + key + " must be a string");
			return def;
		}
		return v.get<std::string>();
	}

	double num(const json &obj, const char *key, const std::string &path, double def, double lo, double hi)
	{
		if (!obj.contains(key))
			return def;
		const auto &v = obj.at(key);
		if (!v.is_number()) {
			errors.push_back(path + "." + key + " must be a number");
			return def;
		}
		const double d = v.get<double>();
		if (!std::isfinite(d) || d < lo || d > hi) {
			errors.push_back(path + "." + key + " out of range [" + std::to_string(lo) + ", " +
					 std::to_string(hi) + "]");
			return def;
		}
		return d;
	}

	int integer(const json &obj, const char *key, const std::string &path, int def, int lo, int hi)
	{
		const double d = num(obj, key, path, def, lo, hi);
		return static_cast<int>(std::lround(d));
	}

	bool boolean(const json &obj, const char *key, const std::string &path, bool def)
	{
		if (!obj.contains(key))
			return def;
		const auto &v = obj.at(key);
		if (!v.is_boolean()) {
			errors.push_back(path + "." + key + " must be true/false");
			return def;
		}
		return v.get<bool>();
	}

	std::vector<std::string> strList(const json &obj, const char *key, const std::string &path)
	{
		std::vector<std::string> out;
		if (!obj.contains(key))
			return out;
		const auto &v = obj.at(key);
		if (!v.is_array()) {
			errors.push_back(path + "." + key + " must be an array of strings");
			return out;
		}
		for (const auto &e : v) {
			if (e.is_string())
				out.push_back(e.get<std::string>());
			else
				errors.push_back(path + "." + key + " must contain only strings");
		}
		return out;
	}

	std::optional<NormRect> rect(const json &v, const std::string &path)
	{
		NormRect r;
		if (v.is_array() && v.size() == 4 &&
		    std::all_of(v.begin(), v.end(), [](const json &e) { return e.is_number(); })) {
			r = NormRect{v[0].get<double>(), v[1].get<double>(), v[2].get<double>(), v[3].get<double>()};
		} else if (v.is_object()) {
			checkKeys(v, path, {"x", "y", "w", "h"});
			r.x = num(v, "x", path, -1, -1e9, 1e9);
			r.y = num(v, "y", path, -1, -1e9, 1e9);
			r.w = num(v, "w", path, -1, -1e9, 1e9);
			r.h = num(v, "h", path, -1, -1e9, 1e9);
		} else {
			errors.push_back(path + " must be {x,y,w,h} or [x,y,w,h]");
			return std::nullopt;
		}
		if (!r.isValid()) {
			errors.push_back(path + " must be a normalized rect inside 0..1 with w,h > 0");
			return std::nullopt;
		}
		return r;
	}

	bool validRegex(const std::string &pattern, const std::string &path)
	{
		try {
			std::regex re(pattern, std::regex::ECMAScript | std::regex::icase);
			(void)re;
			return true;
		} catch (const std::regex_error &e) {
			errors.push_back(path + ": invalid regular expression '" + pattern + "': " + e.what());
			return false;
		}
	}
};

bool isValidId(const std::string &id)
{
	if (id.empty() || id.size() > 64)
		return false;
	return std::all_of(id.begin(), id.end(), [](char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
		       c == '-' || c == '.';
	});
}

void parseTemporal(Reader &rd, const json &t, const std::string &path, TemporalSpec &out)
{
	if (!t.is_object()) {
		rd.errors.push_back(path + " must be an object");
		return;
	}
	rd.checkKeys(t, path,
		     {"confirmFrames", "releaseFrames", "releaseRatio", "cooldownMs", "minEventIntervalMs", "maxBurst",
		      "maxHamming", "textSimilarity", "recentCapacity"});
	out.confirmFrames = rd.integer(t, "confirmFrames", path, out.confirmFrames, 1, 30);
	out.releaseFrames = rd.integer(t, "releaseFrames", path, out.releaseFrames, 1, 60);
	out.releaseRatio = rd.num(t, "releaseRatio", path, out.releaseRatio, 0.0, 1.0);
	out.cooldownMs = rd.integer(t, "cooldownMs", path, out.cooldownMs, 0, 60000);
	out.minEventIntervalMs = rd.integer(t, "minEventIntervalMs", path, out.minEventIntervalMs, 0, 60000);
	out.maxBurst = rd.integer(t, "maxBurst", path, out.maxBurst, 1, 10);
	out.maxHamming = rd.integer(t, "maxHamming", path, out.maxHamming, 0, 64);
	out.textSimilarity = rd.num(t, "textSimilarity", path, out.textSimilarity, 0.0, 1.0);
	out.recentCapacity = rd.integer(t, "recentCapacity", path, out.recentCapacity, 1, 256);
}

void parseDetector(Reader &rd, const json &d, const std::string &path, const std::filesystem::path &baseDir,
		   GameProfile &p)
{
	if (!d.is_object()) {
		rd.errors.push_back(path + " must be an object");
		return;
	}
	DetectorSpec spec;
	spec.id = rd.str(d, "id", path, "", true);
	const std::string type = rd.str(d, "type", path, "", true);
	spec.region = rd.str(d, "region", path, "", true);
	spec.enabled = rd.boolean(d, "enabled", path, true);
	spec.vetoWhenActive = rd.boolean(d, "vetoWhenActive", path, false);
	if (!spec.id.empty() && !isValidId(spec.id))
		rd.errors.push_back(path + ".id contains invalid characters");
	if (!spec.region.empty() && !p.findRegion(spec.region))
		rd.errors.push_back(path + ".region '" + spec.region + "' is not defined in regions");

	const std::string p2 = path + "(" + spec.id + ")";
	if (type == "template") {
		spec.kind = DetectorKind::Template;
		rd.checkKeys(d, p2,
			     {"id", "type", "region", "enabled", "vetoWhenActive", "file", "userTemplate", "threshold",
			      "scales", "maxInstances"});
		spec.file = rd.str(d, "file", p2, "");
		spec.userTemplate = rd.str(d, "userTemplate", p2, "");
		if (spec.file.empty() && spec.userTemplate.empty())
			rd.errors.push_back(p2 + " needs \"file\" or \"userTemplate\"");
		if (!spec.userTemplate.empty() && !isValidId(spec.userTemplate))
			rd.errors.push_back(p2 + ".userTemplate must be a simple name (letters, digits, - and _)");
		spec.threshold = rd.num(d, "threshold", p2, 0.80, 0.1, 1.0);
		spec.maxInstances = rd.integer(d, "maxInstances", p2, 5, 1, 32);
		if (d.contains("scales")) {
			spec.scales.clear();
			const auto &s = d.at("scales");
			if (!s.is_array() || s.empty()) {
				rd.errors.push_back(p2 + ".scales must be a non-empty array of numbers");
			} else {
				for (const auto &v : s) {
					if (!v.is_number() || v.get<double>() < 0.25 || v.get<double>() > 4.0)
						rd.errors.push_back(p2 + ".scales values must be numbers in 0.25..4");
					else
						spec.scales.push_back(v.get<double>());
				}
				if (spec.scales.size() > 8)
					rd.errors.push_back(p2 + ".scales supports at most 8 entries");
			}
		}
		if (!spec.file.empty()) {
			const auto rel = pathFromUtf8(spec.file);
			if (rel.is_absolute() || spec.file.find("..") != std::string::npos) {
				rd.errors.push_back(p2 + ".file must be a relative path inside the profile directory");
			} else {
				// Resolve next to the profile first, then in the parent data
				// directory (so built-in profiles can share data/templates/).
				std::error_code fec;
				spec.resolvedFile = baseDir / rel;
				if (!std::filesystem::exists(spec.resolvedFile, fec) && baseDir.has_parent_path() &&
				    std::filesystem::exists(baseDir.parent_path() / rel, fec))
					spec.resolvedFile = baseDir.parent_path() / rel;
			}
		}
	} else if (type == "ocr") {
		spec.kind = DetectorKind::Ocr;
		rd.checkKeys(d, p2,
			     {"id", "type", "region", "enabled", "vetoWhenActive", "keywords", "patterns", "negativeWords",
			      "fuzzy", "upscale", "refreshMs", "role"});
		spec.role = rd.str(d, "role", p2, "");
		if (!spec.role.empty() && spec.role != "actor" && spec.role != "victim")
			rd.errors.push_back(p2 + ".role must be \"actor\" or \"victim\"");
		spec.keywords = rd.strList(d, "keywords", p2);
		spec.patterns = rd.strList(d, "patterns", p2);
		spec.negativeWords = rd.strList(d, "negativeWords", p2);
		spec.fuzzy = rd.num(d, "fuzzy", p2, 0.80, 0.5, 1.0);
		spec.upscale = rd.num(d, "upscale", p2, 2.0, 1.0, 4.0);
		spec.refreshMs = rd.integer(d, "refreshMs", p2, 1000, 0, 60000);
		for (const auto &pat : spec.patterns) {
			// {gamertag} is substituted (regex-escaped) later; validate with a stand-in.
			std::string probe = pat;
			for (size_t pos; (pos = probe.find(kGamertagPlaceholder)) != std::string::npos;)
				probe.replace(pos, std::char_traits<char>::length(kGamertagPlaceholder), "GAMERTAG");
			rd.validRegex(probe, p2 + ".patterns");
		}
		if (spec.keywords.empty() && spec.patterns.empty())
			rd.errors.push_back(p2 + " needs at least one keyword or pattern");
	} else if (type == "color") {
		spec.kind = DetectorKind::Color;
		rd.checkKeys(d, p2,
			     {"id", "type", "region", "enabled", "vetoWhenActive", "hsv", "minFraction", "maxFraction"});
		if (d.contains("hsv")) {
			const auto &h = d.at("hsv");
			rd.checkKeys(h, p2 + ".hsv", {"hMin", "hMax", "sMin", "sMax", "vMin", "vMax"});
			spec.hsv.hMin = rd.num(h, "hMin", p2 + ".hsv", 0, 0, 360);
			spec.hsv.hMax = rd.num(h, "hMax", p2 + ".hsv", 360, 0, 360);
			spec.hsv.sMin = rd.num(h, "sMin", p2 + ".hsv", 0, 0, 1);
			spec.hsv.sMax = rd.num(h, "sMax", p2 + ".hsv", 1, 0, 1);
			spec.hsv.vMin = rd.num(h, "vMin", p2 + ".hsv", 0, 0, 1);
			spec.hsv.vMax = rd.num(h, "vMax", p2 + ".hsv", 1, 0, 1);
		} else {
			rd.errors.push_back(p2 + ".hsv is required");
		}
		spec.minFraction = rd.num(d, "minFraction", p2, 0.01, 0.0, 1.0);
		spec.maxFraction = rd.num(d, "maxFraction", p2, 1.0, 0.0, 1.0);
		if (spec.maxFraction < spec.minFraction)
			rd.errors.push_back(p2 + ".maxFraction must be >= minFraction");
	} else if (type == "hud_change") {
		spec.kind = DetectorKind::HudChange;
		rd.checkKeys(d, p2, {"id", "type", "region", "enabled", "vetoWhenActive", "threshold", "holdMs"});
		spec.changeThreshold = rd.num(d, "threshold", p2, 10.0, 0.5, 255.0);
		spec.holdMs = rd.integer(d, "holdMs", p2, 1200, 0, 60000);
	} else if (type == "onnx") {
		spec.kind = DetectorKind::Onnx;
		rd.checkKeys(d, p2, {"id", "type", "region", "enabled", "model", "threshold"});
		spec.model = rd.str(d, "model", p2, "", true);
		spec.threshold = rd.num(d, "threshold", p2, 0.5, 0.0, 1.0);
		rd.warnings.push_back(p2 + ": ONNX detectors are planned (Milestone 4) and will report unavailable");
	} else {
		rd.errors.push_back(path + ".type '" + type + "' is not one of template, ocr, color, hud_change, onnx");
		return;
	}
	if (p.findDetector(spec.id))
		rd.errors.push_back(path + ": duplicate detector id '" + spec.id + "'");
	p.detectors.push_back(std::move(spec));
}

void parseTarget(Reader &rd, const json &t, const std::string &path, GameProfile &p)
{
	if (!t.is_object()) {
		rd.errors.push_back(path + " must be an object");
		return;
	}
	rd.checkKeys(t, path,
		     {"id", "event", "signals", "threshold", "minSignals", "requireAny", "requireAll", "temporal", "rules"});
	TargetSpec target;
	const std::string ev = rd.str(t, "event", path, "kill");
	const auto type = parseEventType(ev);
	if (!type || *type == EventType::SessionReset)
		rd.errors.push_back(path + ".event '" + ev + "' is not a detectable event type");
	else
		target.event = *type;
	target.id = rd.str(t, "id", path, ev);
	target.threshold = rd.num(t, "threshold", path, 0.80, 0.05, 1.0);
	target.minSignals = rd.integer(t, "minSignals", path, 2, 1, 16);
	target.requireAny = rd.strList(t, "requireAny", path);
	for (const auto &r : target.requireAny)
		if (!p.findDetector(r))
			rd.errors.push_back(path + ".requireAny references unknown detector '" + r + "'");
	target.requireAll = rd.strList(t, "requireAll", path);
	for (const auto &r : target.requireAll)
		if (!p.findDetector(r))
			rd.errors.push_back(path + ".requireAll references unknown detector '" + r + "'");

	if (!t.contains("signals") || !t.at("signals").is_array() || t.at("signals").empty()) {
		rd.errors.push_back(path + ".signalSpecs must be a non-empty array");
	} else {
		size_t i = 0;
		for (const auto &s : t.at("signals")) {
			const std::string sp = path + ".signalSpecs[" + std::to_string(i++) + "]";
			if (!s.is_object()) {
				rd.errors.push_back(sp + " must be an object");
				continue;
			}
			rd.checkKeys(s, sp, {"detector", "weight", "countInstances", "veto"});
			SignalSpec sig;
			sig.detector = rd.str(s, "detector", sp, "", true);
			sig.weight = rd.num(s, "weight", sp, 0.0, 0.0, 100.0);
			sig.countInstances = rd.boolean(s, "countInstances", sp, false);
			sig.veto = rd.boolean(s, "veto", sp, true);
			if (!p.findDetector(sig.detector))
				rd.errors.push_back(sp + ".detector references unknown detector '" + sig.detector +
						    "'");
			// Weight 0 is fine for "gate" signals that only contribute a veto.
			const DetectorSpec *sd = p.findDetector(sig.detector);
			const bool vetoOnly = sd && sig.veto && (sd->vetoWhenActive || !sd->negativeWords.empty());
			if (sig.weight <= 0.0 && !vetoOnly)
				rd.warnings.push_back(sp + ": weight is 0, signal will not contribute");
			target.signalSpecs.push_back(std::move(sig));
		}
		if (target.minSignals > static_cast<int>(target.signalSpecs.size()))
			rd.errors.push_back(path + ".minSignals is larger than the number of signals");
	}
	if (t.contains("temporal"))
		parseTemporal(rd, t.at("temporal"), path + ".temporal", target.temporal);
	if (t.contains("rules")) {
		const auto &rules = t.at("rules");
		if (!rules.is_array()) {
			rd.errors.push_back(path + ".rules must be an array");
		} else {
			size_t i = 0;
			for (const auto &r : rules) {
				const std::string rp = path + ".rules[" + std::to_string(i++) + "]";
				if (!r.is_object()) {
					rd.errors.push_back(rp + " must be an object");
					continue;
				}
				rd.checkKeys(r, rp, {"event", "textPatterns", "detectorsActive"});
				EventRule rule;
				const auto rt = parseEventType(rd.str(r, "event", rp, "", true));
				if (!rt || *rt == EventType::SessionReset)
					rd.errors.push_back(rp + ".event is not a valid event type");
				else
					rule.event = *rt;
				rule.textPatterns = rd.strList(r, "textPatterns", rp);
				rule.detectorsActive = rd.strList(r, "detectorsActive", rp);
				for (const auto &pat : rule.textPatterns)
					rd.validRegex(pat, rp + ".textPatterns");
				for (const auto &dname : rule.detectorsActive)
					if (!p.findDetector(dname))
						rd.errors.push_back(rp +
								    ".detectorsActive references unknown detector '" +
								    dname + "'");
				if (rule.textPatterns.empty() && rule.detectorsActive.empty())
					rd.errors.push_back(rp + " needs textPatterns or detectorsActive");
				target.rules.push_back(std::move(rule));
			}
		}
	}
	for (const auto &existing : p.targets)
		if (existing.id == target.id)
			rd.errors.push_back(path + ": duplicate target id '" + target.id + "'");
	p.targets.push_back(std::move(target));
}

} // namespace

ProfileParseResult parseProfileJson(const std::string &text, const std::filesystem::path &baseDir)
{
	ProfileParseResult result;
	json root;
	try {
		root = json::parse(text, nullptr, true, /*ignore_comments=*/true);
	} catch (const std::exception &e) {
		result.errors.push_back(std::string("JSON parse error: ") + e.what());
		return result;
	}
	if (!root.is_object()) {
		result.errors.push_back("profile root must be a JSON object");
		return result;
	}

	Reader rd;
	auto p = std::make_shared<GameProfile>();
	p->baseDir = baseDir;
	try {
		rd.checkKeys(root, "profile",
			     {"$schema", "schemaVersion", "id", "displayName", "description", "version", "generic",
			      "match", "reference", "regions", "aspectOverrides", "detectors", "targets"});
		p->schemaVersion = rd.integer(root, "schemaVersion", "profile", 0, 0, 1000);
		if (p->schemaVersion != kProfileSchemaVersion)
			rd.errors.push_back("schemaVersion " + std::to_string(p->schemaVersion) +
					    " is not supported (expected " + std::to_string(kProfileSchemaVersion) +
					    ")");
		p->id = rd.str(root, "id", "profile", "", true);
		if (!p->id.empty() && !isValidId(p->id))
			rd.errors.push_back("profile.id may only contain letters, digits, '_', '-', '.'");
		p->displayName = rd.str(root, "displayName", "profile", p->id);
		p->description = rd.str(root, "description", "profile", "");
		p->version = rd.str(root, "version", "profile", "1");
		p->generic = rd.boolean(root, "generic", "profile", false);

		if (root.contains("match")) {
			const auto &m = root.at("match");
			rd.checkKeys(m, "profile.match", {"processNames", "windowTitleContains", "sourceNameContains"});
			p->match.processNames = rd.strList(m, "processNames", "profile.match");
			p->match.windowTitleContains = rd.strList(m, "windowTitleContains", "profile.match");
			p->match.sourceNameContains = rd.strList(m, "sourceNameContains", "profile.match");
		}
		if (root.contains("reference")) {
			const auto &r = root.at("reference");
			rd.checkKeys(r, "profile.reference", {"width", "height"});
			p->referenceWidth = rd.integer(r, "width", "profile.reference", 1920, 320, 16384);
			p->referenceHeight = rd.integer(r, "height", "profile.reference", 1080, 200, 16384);
		}

		if (!root.contains("regions") || !root.at("regions").is_array() || root.at("regions").empty()) {
			rd.errors.push_back("profile.regions must be a non-empty array");
		} else {
			size_t i = 0;
			for (const auto &r : root.at("regions")) {
				const std::string rp = "profile.regions[" + std::to_string(i++) + "]";
				if (!r.is_object()) {
					rd.errors.push_back(rp + " must be an object");
					continue;
				}
				rd.checkKeys(r, rp, {"id", "rect", "description"});
				RegionSpec reg;
				reg.id = rd.str(r, "id", rp, "", true);
				reg.description = rd.str(r, "description", rp, "");
				if (!reg.id.empty() && !isValidId(reg.id))
					rd.errors.push_back(rp + ".id contains invalid characters");
				if (!r.contains("rect")) {
					rd.errors.push_back(rp + ".rect is required");
					continue;
				}
				if (auto rect = rd.rect(r.at("rect"), rp + ".rect"))
					reg.rect = *rect;
				else
					continue;
				if (p->findRegion(reg.id))
					rd.errors.push_back(rp + ": duplicate region id '" + reg.id + "'");
				p->regions.push_back(std::move(reg));
			}
			if (p->regions.size() > 8)
				rd.errors.push_back("profile.regions supports at most 8 regions");
		}

		if (root.contains("aspectOverrides")) {
			const auto &arr = root.at("aspectOverrides");
			if (!arr.is_array()) {
				rd.errors.push_back("profile.aspectOverrides must be an array");
			} else {
				size_t i = 0;
				for (const auto &o : arr) {
					const std::string op = "profile.aspectOverrides[" + std::to_string(i++) + "]";
					if (!o.is_object()) {
						rd.errors.push_back(op + " must be an object");
						continue;
					}
					rd.checkKeys(o, op, {"aspect", "regions"});
					AspectOverride ov;
					const std::string aspectText = rd.str(o, "aspect", op, "", true);
					ov.aspect = parseAspect(aspectText.c_str());
					if (ov.aspect <= 0.0)
						rd.errors.push_back(op + ".aspect must look like \"21:9\" or \"2.37\"");
					if (o.contains("regions") && o.at("regions").is_object()) {
						for (auto it = o.at("regions").begin(); it != o.at("regions").end();
						     ++it) {
							if (!p->findRegion(it.key())) {
								rd.errors.push_back(op + ".regions: unknown region '" +
										    it.key() + "'");
								continue;
							}
							if (auto rect =
								    rd.rect(it.value(), op + ".regions." + it.key()))
								ov.regions[it.key()] = *rect;
						}
					} else {
						rd.errors.push_back(op + ".regions must be an object {regionId: rect}");
					}
					p->aspectOverrides.push_back(std::move(ov));
				}
			}
		}

		if (!root.contains("detectors") || !root.at("detectors").is_array() || root.at("detectors").empty()) {
			rd.errors.push_back("profile.detectors must be a non-empty array");
		} else {
			size_t i = 0;
			for (const auto &d : root.at("detectors"))
				parseDetector(rd, d, "profile.detectors[" + std::to_string(i++) + "]", baseDir, *p);
			if (p->detectors.size() > 32)
				rd.errors.push_back("profile.detectors supports at most 32 detectors");
		}

		if (!root.contains("targets") || !root.at("targets").is_array() || root.at("targets").empty()) {
			rd.errors.push_back("profile.targets must be a non-empty array");
		} else {
			size_t i = 0;
			for (const auto &t : root.at("targets"))
				parseTarget(rd, t, "profile.targets[" + std::to_string(i++) + "]", *p);
		}
	} catch (const std::exception &e) {
		rd.errors.push_back(std::string("unexpected profile error: ") + e.what());
	}

	result.errors = std::move(rd.errors);
	result.warnings = std::move(rd.warnings);
	result.ok = result.errors.empty();
	if (result.ok)
		result.profile = std::move(p);
	return result;
}

ProfileParseResult loadProfileFile(const std::filesystem::path &file)
{
	std::string text;
	if (!readTextFile(file, text, 2u * 1024u * 1024u)) {
		ProfileParseResult r;
		r.errors.push_back("cannot read file (missing or larger than 2 MiB)");
		return r;
	}
	auto result = parseProfileJson(text, file.parent_path());
	if (result.ok)
		result.profile->sourcePath = file;
	return result;
}

std::shared_ptr<GameProfile> applyOverrides(const GameProfile &profile, const ProfileOverrides &ov)
{
	auto copy = std::make_shared<GameProfile>(profile);
	for (auto &t : copy->targets) {
		if (ov.threshold)
			t.threshold = std::clamp(*ov.threshold, 0.05, 1.0);
		if (ov.cooldownMs)
			t.temporal.cooldownMs = std::clamp(*ov.cooldownMs, 0, 60000);
	}
	if (ov.primaryRegion && ov.primaryRegion->isValid() && !copy->regions.empty()) {
		copy->regions.front().rect = *ov.primaryRegion;
		// An explicit user ROI wins over aspect overrides for that region.
		for (auto &a : copy->aspectOverrides)
			a.regions.erase(copy->regions.front().id);
	}
	return copy;
}

namespace {
bool containsPlaceholder(const std::vector<std::string> &list)
{
	for (const auto &s : list)
		if (s.find(kGamertagPlaceholder) != std::string::npos)
			return true;
	return false;
}

std::string replaceAll(std::string s, const std::string &what, const std::string &with)
{
	for (size_t pos = 0; (pos = s.find(what, pos)) != std::string::npos; pos += with.size())
		s.replace(pos, what.size(), with);
	return s;
}

std::string regexEscape(const std::string &s)
{
	static const std::string special = R"(\^$.|?*+()[]{}/-)";
	std::string out;
	for (char c : s) {
		if (special.find(c) != std::string::npos)
			out += '\\';
		out += c;
	}
	return out;
}

bool detectorUsesGamertag(const DetectorSpec &d)
{
	return containsPlaceholder(d.keywords) || containsPlaceholder(d.patterns) || containsPlaceholder(d.negativeWords);
}
} // namespace

bool profileUsesGamertag(const GameProfile &profile)
{
	for (const auto &d : profile.detectors)
		if (d.enabled && detectorUsesGamertag(d))
			return true;
	return false;
}

std::filesystem::path userTemplatePath(const std::filesystem::path &userTemplateDir, const std::string &profileId,
				       const std::string &name)
{
	return userTemplateDir / pathFromUtf8(profileId) / pathFromUtf8(name + ".png");
}

PersonalizeResult personalizeProfile(const GameProfile &profile, const Personalization &pers)
{
	PersonalizeResult r;
	r.profile = std::make_shared<GameProfile>(profile);
	std::string tag = pers.gamertag;
	// Trim surrounding whitespace.
	while (!tag.empty() && std::isspace(static_cast<unsigned char>(tag.front())))
		tag.erase(tag.begin());
	while (!tag.empty() && std::isspace(static_cast<unsigned char>(tag.back())))
		tag.pop_back();
	for (auto &d : r.profile->detectors) {
		if (!d.enabled)
			continue;
		if (detectorUsesGamertag(d)) {
			if (tag.empty()) {
				r.needsGamertag = true;
				d.enabled = false; // reported as unavailable by fusion
				r.warnings.push_back("detector '" + d.id + "' needs your gamertag (Settings)");
				continue;
			}
			for (auto &k : d.keywords)
				k = replaceAll(k, kGamertagPlaceholder, tag);
			for (auto &k : d.negativeWords)
				k = replaceAll(k, kGamertagPlaceholder, tag);
			for (auto &k : d.patterns)
				k = replaceAll(k, kGamertagPlaceholder, regexEscape(tag));
		}
		if (d.kind == DetectorKind::Template && !d.userTemplate.empty()) {
			std::error_code ec;
			const auto path = pers.userTemplateDir.empty()
						  ? std::filesystem::path()
						  : userTemplatePath(pers.userTemplateDir, profile.id, d.userTemplate);
			if (!path.empty() && std::filesystem::exists(path, ec)) {
				d.resolvedFile = path;
			} else if (d.file.empty()) {
				r.missingUserTemplates.push_back(d.userTemplate);
				d.resolvedFile = path; // missing file -> detector reports unavailable
				r.warnings.push_back("detector '" + d.id + "' needs a captured '" + d.userTemplate +
						     "' image (dock: Capture my name plate)");
			}
		}
	}
	return r;
}

void ProfileRegistry::loadDir(const std::filesystem::path &dir, bool builtin, std::map<std::string, ProfilePtr> &out,
			      std::vector<ProfileLoadIssue> &issues)
{
	std::error_code ec;
	if (!std::filesystem::is_directory(dir, ec))
		return;
	std::vector<std::filesystem::path> files;
	for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
		if (it->is_regular_file(ec) && it->path().extension() == ".json")
			files.push_back(it->path());
	}
	std::sort(files.begin(), files.end());
	for (const auto &f : files) {
		auto r = loadProfileFile(f);
		if (!r.errors.empty() || !r.warnings.empty())
			issues.push_back({pathToUtf8(f), r.errors, r.warnings});
		if (!r.ok) {
			KS_LOG_WARN("profile '%s' rejected: %s", pathToUtf8(f).c_str(),
				    r.errors.empty() ? "?" : r.errors.front().c_str());
			continue;
		}
		r.profile->builtin = builtin;
		if (out.count(r.profile->id))
			KS_LOG_INFO("profile '%s' from %s overrides an earlier profile", r.profile->id.c_str(),
				    pathToUtf8(f).c_str());
		out[r.profile->id] = r.profile;
	}
}

void ProfileRegistry::loadDirectories(const std::vector<std::filesystem::path> &builtinDirs,
				      const std::vector<std::filesystem::path> &userDirs)
{
	std::map<std::string, ProfilePtr> loaded;
	std::vector<ProfileLoadIssue> issues;
	for (const auto &d : builtinDirs)
		loadDir(d, true, loaded, issues);
	for (const auto &d : userDirs)
		loadDir(d, false, loaded, issues);
	std::lock_guard<std::mutex> lock(mutex_);
	profiles_ = std::move(loaded);
	issues_ = std::move(issues);
	KS_LOG_INFO("loaded %zu profile(s), %zu file(s) with issues", profiles_.size(), issues_.size());
}

std::vector<ProfilePtr> ProfileRegistry::list() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	std::vector<ProfilePtr> out;
	for (const auto &kv : profiles_)
		out.push_back(kv.second);
	return out;
}

ProfilePtr ProfileRegistry::get(const std::string &id) const
{
	std::lock_guard<std::mutex> lock(mutex_);
	auto it = profiles_.find(id);
	return it == profiles_.end() ? nullptr : it->second;
}

std::vector<ProfileLoadIssue> ProfileRegistry::issues() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return issues_;
}

size_t ProfileRegistry::size() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return profiles_.size();
}

} // namespace killsense
