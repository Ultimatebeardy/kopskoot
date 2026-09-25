// REALM KillSense - detection engine (single-threaded; owned by the worker).
// Pipeline per frame:
//   regions -> preprocessing (gray + dHash) -> detectors -> evidence fusion
//   -> temporal confirmation / duplicate suppression -> event type rules
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "killsense/capture.hpp"
#include "killsense/detector.hpp"
#include "killsense/fusion.hpp"
#include "killsense/profile.hpp"
#include "killsense/temporal.hpp"
#include "killsense/text-match.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace killsense {

struct DetectedEvent {
	EventType type = EventType::Kill;
	std::string targetId;
	double confidence = 0.0;
	std::string reason;
	std::vector<EvidenceItem> evidence;
	int64_t frameTimeMs = 0;
};

struct TargetAnalysis {
	std::string targetId;
	EventType event = EventType::Kill;
	FusionResult fusion;
	TemporalDecision decision;
};

struct FrameAnalysis {
	uint64_t sequence = 0;
	int64_t timeMs = 0;
	double processingMs = 0.0;
	std::vector<DetectorOutput> detectors;
	std::vector<TargetAnalysis> targets;
	std::vector<DetectedEvent> events;
};

// Converts an engine detection into the public event schema (counters are
// filled in later by SessionTracker::record).
KillEvent makeKillEvent(const DetectedEvent &det, const GameProfile *profile, const char *detector = "vision");

class DetectionEngine {
public:
	DetectionEngine();
	~DetectionEngine();
	DetectionEngine(const DetectionEngine &) = delete;
	DetectionEngine &operator=(const DetectionEngine &) = delete;

	// (Re)builds detectors for the profile. `ocr` may be nullptr and must
	// outlive the engine configuration.
	void configure(ProfilePtr profile, IOcrEngine *ocr, const PresetSettings &preset);
	ProfilePtr profile() const { return profile_; }
	bool configured() const { return static_cast<bool>(profile_); }

	void process(const FramePacket &packet, FrameAnalysis &out);
	void resetTemporal();

	// Detector ids that cannot run (missing OCR, missing template, planned kinds).
	std::vector<std::string> unavailableDetectors() const;

private:
	struct TargetState {
		const TargetSpec *spec = nullptr;
		std::unique_ptr<TemporalTracker> tracker;
		std::vector<CompiledPatterns> rulePatterns; // parallel to spec->rules
		std::string hashRegion;                     // region used for the visual fingerprint
	};
	struct RegionCache {
		Image gray;
		uint64_t hash = 0;
	};

	EventType classify(const TargetState &ts, const std::vector<DetectorOutput> &outputs) const;

	ProfilePtr profile_;
	PresetSettings preset_;
	std::vector<std::unique_ptr<IDetector>> detectors_;
	std::vector<TargetState> targets_;
	std::map<std::string, RegionCache> regionCache_;
};

} // namespace killsense
