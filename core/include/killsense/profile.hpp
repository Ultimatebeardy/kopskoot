// REALM KillSense - game profile model, parser and registry.
// Profiles are JSON files stored outside the binary (see docs/PROFILE_SCHEMA.md).
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "killsense/event.hpp"
#include "killsense/geometry.hpp"

#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace killsense {

struct RegionSpec {
	std::string id;
	NormRect rect;
	std::string description;
};

struct AspectOverride {
	double aspect = 0.0; // width / height, e.g. 2.37 for 21:9
	std::map<std::string, NormRect> regions;
};

struct HsvRange {
	double hMin = 0.0;   // degrees 0..360 (hMin > hMax wraps around red)
	double hMax = 360.0; // degrees
	double sMin = 0.0;   // 0..1
	double sMax = 1.0;
	double vMin = 0.0; // 0..1
	double vMax = 1.0;
};

enum class DetectorKind { Template, Ocr, Color, HudChange, Onnx };
const char *detectorKindId(DetectorKind kind);

struct DetectorSpec {
	std::string id;
	DetectorKind kind = DetectorKind::Template;
	std::string region;
	bool enabled = true;

	// template
	std::string file;
	std::filesystem::path resolvedFile;
	double threshold = 0.80;
	std::vector<double> scales{1.0};
	int maxInstances = 5;

	// ocr
	std::vector<std::string> keywords;
	std::vector<std::string> patterns;
	std::vector<std::string> negativeWords;
	double fuzzy = 0.80;  // minimum similarity for keyword matches
	double upscale = 2.0; // OCR input upscale factor (small HUD text)
	int refreshMs = 1000; // re-run OCR at least this often even if region unchanged

	// color
	HsvRange hsv;
	double minFraction = 0.01;
	double maxFraction = 1.0;

	// hud_change
	double changeThreshold = 10.0; // mean abs diff (0..255) that counts as a change
	int holdMs = 1200;             // how long a change keeps contributing evidence

	// onnx (planned - Milestone 4)
	std::string model;
};

struct SignalSpec {
	std::string detector;
	double weight = 0.0;
	bool countInstances = false; // use detector instance count for rapid kills
	bool veto = true;            // honour detector veto (e.g. OCR negative words)
};

struct EventRule {
	EventType event = EventType::Kill;
	std::vector<std::string> textPatterns;    // regex on OCR text of the target's OCR signals
	std::vector<std::string> detectorsActive; // all listed detectors must be active
};

struct TemporalSpec {
	int confirmFrames = 2;        // consecutive passing frames before an event fires
	int releaseFrames = 3;        // consecutive failing frames before the episode ends
	double releaseRatio = 0.75;   // confidence < threshold*ratio counts as failing
	int cooldownMs = 1500;        // same-fingerprint re-appearance suppressed within this gap
	int minEventIntervalMs = 150; // global debounce between two events of this target
	int maxBurst = 4;             // max events emitted from a single instance-count jump
	int maxHamming = 8;           // dHash distance treated as "same visual"
	double textSimilarity = 0.80; // OCR text similarity treated as "same message"
	int recentCapacity = 16;      // number of fingerprints remembered
};

struct TargetSpec {
	std::string id;
	EventType event = EventType::Kill;
	std::vector<SignalSpec> signalSpecs;
	double threshold = 0.80;
	int minSignals = 2;
	std::vector<std::string> requireAny; // at least one of these detectors must be active
	TemporalSpec temporal;
	std::vector<EventRule> rules;
};

struct MatchHints {
	std::vector<std::string> processNames;        // e.g. "Game.exe" (from OBS capture settings only)
	std::vector<std::string> windowTitleContains; // matched against OBS capture settings
	std::vector<std::string> sourceNameContains;  // matched against the OBS source name
};

struct GameProfile {
	int schemaVersion = 1;
	std::string id;
	std::string displayName;
	std::string description;
	std::string version;
	bool generic = false;
	MatchHints match;
	int referenceWidth = 1920;
	int referenceHeight = 1080;
	std::vector<RegionSpec> regions;
	std::vector<AspectOverride> aspectOverrides;
	std::vector<DetectorSpec> detectors;
	std::vector<TargetSpec> targets;

	std::filesystem::path baseDir; // directory used to resolve relative files
	std::filesystem::path sourcePath;
	bool builtin = false;

	const RegionSpec *findRegion(const std::string &id) const;
	const DetectorSpec *findDetector(const std::string &id) const;
	// Regions with aspect override applied (override chosen within 2% of aspect).
	std::map<std::string, NormRect> resolveRegions(double sourceAspect) const;
};

using ProfilePtr = std::shared_ptr<const GameProfile>;

struct ProfileParseResult {
	bool ok = false;
	std::shared_ptr<GameProfile> profile;
	std::vector<std::string> errors;
	std::vector<std::string> warnings;
};

// Parses and validates. Never throws; malformed input yields ok=false + errors.
ProfileParseResult parseProfileJson(const std::string &text, const std::filesystem::path &baseDir);
ProfileParseResult loadProfileFile(const std::filesystem::path &file);

// Optional user overrides (from plugin settings) applied to every target.
struct ProfileOverrides {
	std::optional<double> threshold;
	std::optional<int> cooldownMs;
	std::optional<NormRect> primaryRegion; // replaces the first region's rect
};
std::shared_ptr<GameProfile> applyOverrides(const GameProfile &profile, const ProfileOverrides &ov);

struct ProfileLoadIssue {
	std::string file;
	std::vector<std::string> errors;
	std::vector<std::string> warnings;
};

// Thread-safe collection of loaded profiles. User profiles override built-in
// profiles with the same id.
class ProfileRegistry {
public:
	void loadDirectories(const std::vector<std::filesystem::path> &builtinDirs,
			     const std::vector<std::filesystem::path> &userDirs);
	std::vector<ProfilePtr> list() const;
	ProfilePtr get(const std::string &id) const;
	std::vector<ProfileLoadIssue> issues() const;
	size_t size() const;

private:
	void loadDir(const std::filesystem::path &dir, bool builtin, std::map<std::string, ProfilePtr> &out,
		     std::vector<ProfileLoadIssue> &issues);

	mutable std::mutex mutex_;
	std::map<std::string, ProfilePtr> profiles_;
	std::vector<ProfileLoadIssue> issues_;
};

} // namespace killsense
