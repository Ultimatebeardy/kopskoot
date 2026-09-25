// REALM KillSense - offline replay of recorded screenshots / image sequences.
// Runs the exact capture planning + detection pipeline used inside OBS, but
// synchronously and deterministically, so detection logic can be tested
// without OBS or a game.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "killsense/capture.hpp"
#include "killsense/engine.hpp"
#include "killsense/event.hpp"
#include "killsense/profile.hpp"
#include "killsense/session.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace killsense {

enum class ReplayOcrMode {
	None,     // OCR detectors unavailable
	Sidecar,  // read <frame>.ocr.txt next to each frame (deterministic tests)
	Platform, // real platform OCR (Windows.Media.Ocr on Windows)
};

struct ReplayOptions {
	std::filesystem::path profileFile;
	std::filesystem::path framesDir; // *.png/*.jpg/*.bmp sorted by file name
	double fps = 6.0;                // timestamps assigned to frames
	PerformancePreset preset = PerformancePreset::Balanced;
	ReplayOcrMode ocr = ReplayOcrMode::Sidecar;
	std::string ocrLanguage;
	ProfileOverrides overrides;
	bool collectFrameLog = false;
};

struct ReplayResult {
	bool ok = false;
	std::string error;
	int frames = 0;
	std::vector<KillEvent> events;
	SessionStats session;
	std::vector<std::string> frameLog; // one line per frame when requested
};

ReplayResult runReplay(const ReplayOptions &options);

} // namespace killsense
