// SPDX-License-Identifier: GPL-2.0-or-later
#include "killsense/config.hpp"

#include "killsense/fsutil.hpp"
#include "killsense/version.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>

namespace killsense {

using json = nlohmann::json;

PresetSettings AppConfig::effectivePreset() const
{
	PresetSettings p = presetSettings(preset);
	if (analysisFpsOverride > 0.0)
		p.analysisFps = std::clamp(analysisFpsOverride, 0.5, 30.0);
	return p;
}

std::string configToJson(const AppConfig &c)
{
	nlohmann::ordered_json j;
	j["schemaVersion"] = kConfigSchemaVersion;
	j["sourceUuid"] = c.sourceUuid;
	j["sourceName"] = c.sourceName;
	j["profileId"] = c.profileId;
	j["autoStart"] = c.autoStart;
	j["preset"] = presetId(c.preset);
	j["analysisFpsOverride"] = c.analysisFpsOverride;
	j["thresholdOverride"] = c.thresholdOverride ? json(*c.thresholdOverride) : json(nullptr);
	j["cooldownOverrideMs"] = c.cooldownOverrideMs ? json(*c.cooldownOverrideMs) : json(nullptr);
	j["customRoi"] = {{"enabled", c.customRoiEnabled},
			  {"x", c.customRoi.x},
			  {"y", c.customRoi.y},
			  {"w", c.customRoi.w},
			  {"h", c.customRoi.h}};
	j["outputs"] = {{"websocketVendor", c.outputWebsocketVendor},
			{"jsonlFile", c.outputJsonl},
			{"jsonlPath", c.jsonlPath}};
	j["ocrLanguage"] = c.ocrLanguage;
	j["gamertag"] = c.gamertag;
	j["counter"] = {{"textSource", c.counterTextSource}, {"format", c.counterFormat}};
	j["overlay"] = {{"popups", c.overlayPopups}};
	j["logLevel"] = logLevelName(c.logLevel);
	j["telemetry"] = c.telemetry;
	j["hotkeys"] = c.hotkeysJson;
	return j.dump(2);
}

namespace {
template<typename T> T get(const json &j, const char *key, T def, std::vector<std::string> &warnings)
{
	if (!j.contains(key))
		return def;
	try {
		return j.at(key).get<T>();
	} catch (const std::exception &) {
		warnings.push_back(std::string("config: '") + key + "' has the wrong type; using default");
		return def;
	}
}
} // namespace

ConfigLoadResult configFromJson(const std::string &text)
{
	ConfigLoadResult r;
	AppConfig &c = r.config;
	json j;
	try {
		j = json::parse(text);
	} catch (const std::exception &e) {
		r.usedDefaults = true;
		r.warnings.push_back(std::string("config: parse error, using defaults: ") + e.what());
		return r;
	}
	if (!j.is_object()) {
		r.usedDefaults = true;
		r.warnings.push_back("config: root is not an object, using defaults");
		return r;
	}
	auto &w = r.warnings;
	const int version = get<int>(j, "schemaVersion", 0, w);
	if (version > kConfigSchemaVersion)
		w.push_back("config: written by a newer version; unknown fields are ignored");
	// Future migrations: if (version < 2) { ... }

	c.sourceUuid = get<std::string>(j, "sourceUuid", "", w);
	c.sourceName = get<std::string>(j, "sourceName", "", w);
	c.profileId = get<std::string>(j, "profileId", "generic", w);
	c.autoStart = get<bool>(j, "autoStart", false, w);
	PerformancePreset preset;
	if (parsePreset(get<std::string>(j, "preset", "balanced", w), preset))
		c.preset = preset;
	else
		w.push_back("config: unknown preset, using balanced");
	const double fps = get<double>(j, "analysisFpsOverride", 0.0, w);
	c.analysisFpsOverride = std::isfinite(fps) && fps >= 0.0 && fps <= 30.0 ? fps : 0.0;
	if (j.contains("thresholdOverride") && j.at("thresholdOverride").is_number()) {
		const double t = j.at("thresholdOverride").get<double>();
		if (t >= 0.05 && t <= 1.0)
			c.thresholdOverride = t;
	}
	if (j.contains("cooldownOverrideMs") && j.at("cooldownOverrideMs").is_number_integer()) {
		const int cd = j.at("cooldownOverrideMs").get<int>();
		if (cd >= 0 && cd <= 60000)
			c.cooldownOverrideMs = cd;
	}
	if (j.contains("customRoi") && j.at("customRoi").is_object()) {
		const auto &roi = j.at("customRoi");
		c.customRoiEnabled = get<bool>(roi, "enabled", false, w);
		NormRect nr{get<double>(roi, "x", c.customRoi.x, w), get<double>(roi, "y", c.customRoi.y, w),
			    get<double>(roi, "w", c.customRoi.w, w), get<double>(roi, "h", c.customRoi.h, w)};
		if (nr.isValid())
			c.customRoi = nr;
		else {
			c.customRoiEnabled = false;
			w.push_back("config: customRoi invalid, disabled");
		}
	}
	if (j.contains("outputs") && j.at("outputs").is_object()) {
		const auto &o = j.at("outputs");
		c.outputWebsocketVendor = get<bool>(o, "websocketVendor", true, w);
		c.outputJsonl = get<bool>(o, "jsonlFile", false, w);
		c.jsonlPath = get<std::string>(o, "jsonlPath", "", w);
	}
	c.ocrLanguage = get<std::string>(j, "ocrLanguage", "", w);
	c.gamertag = get<std::string>(j, "gamertag", "", w);
	if (j.contains("overlay") && j.at("overlay").is_object())
		c.overlayPopups = get<bool>(j.at("overlay"), "popups", true, w);
	if (c.gamertag.size() > 64)
		c.gamertag.resize(64);
	if (j.contains("counter") && j.at("counter").is_object()) {
		const auto &o = j.at("counter");
		c.counterTextSource = get<std::string>(o, "textSource", "", w);
		c.counterFormat = get<std::string>(o, "format", c.counterFormat, w);
		if (c.counterFormat.size() > 256)
			c.counterFormat.resize(256);
	}
	LogLevel lvl;
	if (parseLogLevel(get<std::string>(j, "logLevel", "info", w), lvl))
		c.logLevel = lvl;
	c.telemetry = get<bool>(j, "telemetry", false, w);
	c.hotkeysJson = get<std::string>(j, "hotkeys", "", w);
	r.loadedFromFile = true;
	return r;
}

ConfigLoadResult loadConfigFile(const std::filesystem::path &file)
{
	std::error_code ec;
	if (!std::filesystem::exists(file, ec)) {
		ConfigLoadResult r;
		r.usedDefaults = true;
		return r;
	}
	std::string text;
	if (!readTextFile(file, text, 1024u * 1024u)) {
		ConfigLoadResult r;
		r.usedDefaults = true;
		r.warnings.push_back("config: unreadable, using defaults");
		return r;
	}
	auto r = configFromJson(text);
	if (r.usedDefaults) {
		// Keep the broken file for the user to inspect instead of overwriting it silently.
		std::filesystem::path backup = file;
		backup += ".invalid";
		std::filesystem::copy_file(file, backup, std::filesystem::copy_options::overwrite_existing, ec);
	}
	return r;
}

bool saveConfigFile(const std::filesystem::path &file, const AppConfig &cfg)
{
	return writeTextFileAtomic(file, configToJson(cfg));
}

} // namespace killsense
