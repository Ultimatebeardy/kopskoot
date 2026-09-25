// SPDX-License-Identifier: GPL-2.0-or-later
#include "killsense/engine.hpp"

#include "killsense/logging.hpp"

#include <chrono>

namespace killsense {

KillEvent makeKillEvent(const DetectedEvent &det, const GameProfile *profile, const char *detector)
{
	KillEvent ev;
	ev.type = det.type;
	ev.profile = profile ? profile->id : std::string("none");
	ev.game = profile ? profile->displayName : std::string("unknown");
	ev.target = det.targetId;
	ev.confidence = det.confidence;
	ev.detector = detector ? detector : "vision";
	ev.reason = det.reason;
	ev.evidence = det.evidence;
	ev.timestampMs = nowUnixMs();
	ev.timestamp = isoTimestampUtc(ev.timestampMs);
	return ev;
}

DetectionEngine::DetectionEngine() = default;
DetectionEngine::~DetectionEngine() = default;

void DetectionEngine::configure(ProfilePtr profile, IOcrEngine *ocr, const PresetSettings &preset)
{
	detectors_.clear();
	targets_.clear();
	regionCache_.clear();
	profile_ = std::move(profile);
	preset_ = preset;
	if (!profile_)
		return;

	DetectorDeps deps;
	deps.ocr = ocr;
	for (const auto &spec : profile_->detectors) {
		if (!spec.enabled)
			continue;
		auto det = createDetector(spec, deps);
		if (!det->available())
			KS_LOG_WARN("profile '%s': detector '%s' (%s) unavailable: %s", profile_->id.c_str(),
				    spec.id.c_str(), detectorKindId(spec.kind), det->unavailableReason().c_str());
		detectors_.push_back(std::move(det));
	}
	for (const auto &t : profile_->targets) {
		TargetState ts;
		ts.spec = &t;
		bool useInstances = false;
		for (const auto &s : t.signalSpecs)
			useInstances = useInstances || s.countInstances;
		ts.tracker = std::make_unique<TemporalTracker>(t.temporal, t.threshold, useInstances);
		for (const auto &rule : t.rules)
			ts.rulePatterns.push_back(compilePatterns(rule.textPatterns));
		if (!t.signalSpecs.empty())
			if (const auto *d = profile_->findDetector(t.signalSpecs.front().detector))
				ts.hashRegion = d->region;
		targets_.push_back(std::move(ts));
	}
	KS_LOG_INFO("engine configured: profile '%s', %zu detector(s), %zu target(s)", profile_->id.c_str(),
		    detectors_.size(), targets_.size());
}

void DetectionEngine::resetTemporal()
{
	for (auto &t : targets_)
		t.tracker->reset();
	for (auto &d : detectors_)
		d->reset();
}

std::vector<std::string> DetectionEngine::unavailableDetectors() const
{
	std::vector<std::string> out;
	for (const auto &d : detectors_)
		if (!d->available())
			out.push_back(d->spec().id + " (" + d->unavailableReason() + ")");
	return out;
}

EventType DetectionEngine::classify(const TargetState &ts, const std::vector<DetectorOutput> &outputs) const
{
	const TargetSpec &t = *ts.spec;
	for (size_t i = 0; i < t.rules.size(); ++i) {
		const auto &rule = t.rules[i];
		bool ok = true;
		for (const auto &id : rule.detectorsActive) {
			const DetectorOutput *o = findOutput(outputs, id);
			if (!o || !o->available || !o->active) {
				ok = false;
				break;
			}
		}
		if (!ok)
			continue;
		if (!rule.textPatterns.empty()) {
			bool textOk = false;
			for (const auto &sig : t.signalSpecs) {
				const DetectorOutput *o = findOutput(outputs, sig.detector);
				if (o && o->kind == DetectorKind::Ocr && o->available &&
				    (anyPatternMatches(ts.rulePatterns[i], o->rawText) ||
				     anyPatternMatches(ts.rulePatterns[i], o->text))) {
					textOk = true;
					break;
				}
			}
			if (!textOk)
				continue;
		}
		return rule.event;
	}
	return t.event;
}

void DetectionEngine::process(const FramePacket &packet, FrameAnalysis &out)
{
	const auto t0 = std::chrono::steady_clock::now();
	out.sequence = packet.sequence;
	out.timeMs = packet.captureTimeMs;
	out.detectors.clear();
	out.targets.clear();
	out.events.clear();
	if (!profile_)
		return;

	// Preprocess each captured region once (gray + perceptual hash).
	for (const auto &rc : packet.regions) {
		auto &cache = regionCache_[rc.regionId];
		toGray(rc.rgba, cache.gray);
		cache.hash = dHash64(cache.gray);
	}

	for (auto &det : detectors_) {
		DetectorOutput o;
		o.detectorId = det->spec().id;
		o.kind = det->spec().kind;
		o.regionId = det->spec().region;
		const RegionCapture *rc = packet.findRegion(o.regionId);
		if (!rc || rc->rgba.empty()) {
			o.available = det->available();
			o.detail = "region not captured";
			out.detectors.push_back(std::move(o));
			continue;
		}
		const auto &cache = regionCache_[o.regionId];
		DetectorContext ctx;
		ctx.timeMs = packet.captureTimeMs;
		ctx.region = rc;
		ctx.gray = &cache.gray;
		ctx.regionHash = cache.hash;
		ctx.ocrMinIntervalMs = preset_.ocrMinIntervalMs;
		try {
			det->process(ctx, o);
		} catch (const std::exception &e) {
			// A failing detector must never take the pipeline (or OBS) down.
			o.available = false;
			o.detail = std::string("exception: ") + e.what();
			KS_LOG_WARN("detector '%s' threw: %s", o.detectorId.c_str(), e.what());
		}
		out.detectors.push_back(std::move(o));
	}

	for (auto &ts : targets_) {
		TargetAnalysis ta;
		ta.targetId = ts.spec->id;
		ta.event = ts.spec->event;
		ta.fusion = fuseTarget(*ts.spec, out.detectors);

		Observation obs;
		obs.timeMs = packet.captureTimeMs;
		obs.confidence = ta.fusion.confidence;
		obs.passes = ta.fusion.passes;
		obs.instanceCount = ta.fusion.instanceCount;
		auto it = regionCache_.find(ts.hashRegion);
		obs.visualHash = it != regionCache_.end() ? it->second.hash : 0;
		for (const auto &sig : ts.spec->signalSpecs) {
			const DetectorOutput *o = findOutput(out.detectors, sig.detector);
			if (o && o->kind == DetectorKind::Ocr && o->available && !o->text.empty()) {
				if (!obs.text.empty())
					obs.text += ' ';
				obs.text += o->text;
			}
		}
		ta.decision = ts.tracker->update(obs);

		if (ta.decision.emitCount > 0) {
			const EventType type = classify(ts, out.detectors);
			for (int i = 0; i < ta.decision.emitCount; ++i) {
				DetectedEvent ev;
				ev.type = type;
				ev.targetId = ts.spec->id;
				ev.confidence = ta.fusion.confidence;
				ev.reason = ta.decision.reason;
				ev.frameTimeMs = packet.captureTimeMs;
				for (const auto &c : ta.fusion.contributions) {
					if (!c.available)
						continue;
					EvidenceItem e;
					e.detector = c.detector;
					e.kind = detectorKindId(c.kind);
					e.weight = c.weight;
					e.score = c.score;
					e.contribution = c.contribution;
					e.active = c.active;
					ev.evidence.push_back(std::move(e));
				}
				out.events.push_back(std::move(ev));
			}
		}
		out.targets.push_back(std::move(ta));
	}
	out.processingMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

} // namespace killsense
