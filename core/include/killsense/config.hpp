// REALM KillSense - versioned plugin configuration (config.json).
// Stored in the OBS plugin config directory, e.g.
//   %APPDATA%\obs-studio\plugin_config\realm-killsense\config.json
// Invalid files never crash OBS: they are backed up and defaults are used.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "killsense/capture.hpp"
#include "killsense/geometry.hpp"
#include "killsense/logging.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace killsense {

struct AppConfig {
	int schemaVersion = 1;
	// Source is stored by UUID (stable across renames) with the name as fallback.
	std::string sourceUuid;
	std::string sourceName;
	std::string profileId = "generic";
	bool autoStart = false; // start detection when OBS finishes loading

	PerformancePreset preset = PerformancePreset::Balanced;
	double analysisFpsOverride = 0.0; // 0 = use preset

	std::optional<double> thresholdOverride;
	std::optional<int> cooldownOverrideMs;
	bool customRoiEnabled = false;
	NormRect customRoi{0.70, 0.05, 0.28, 0.25};

	bool outputWebsocketVendor = true;
	bool outputJsonl = false;
	std::string jsonlPath;

	std::string ocrLanguage; // "" = Windows user profile languages
	LogLevel logLevel = LogLevel::Info;
	bool telemetry = false; // opt-in; no telemetry is implemented or sent

	std::string hotkeysJson; // opaque obs_data JSON written by the plugin

	PresetSettings effectivePreset() const;
};

struct ConfigLoadResult {
	AppConfig config;
	bool loadedFromFile = false;
	bool usedDefaults = false;
	std::vector<std::string> warnings;
};

std::string configToJson(const AppConfig &cfg);
// Never throws. Unknown/invalid fields fall back to defaults with warnings.
ConfigLoadResult configFromJson(const std::string &text);
ConfigLoadResult loadConfigFile(const std::filesystem::path &file);
bool saveConfigFile(const std::filesystem::path &file, const AppConfig &cfg);

} // namespace killsense
