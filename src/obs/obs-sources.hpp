// REALM KillSense - OBS source enumeration and non-invasive game identification.
// Game identification only reads OBS's OWN capture-source settings (the window
// a Game/Window Capture source was configured for). No process inspection,
// memory reading or injection of any kind.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "obs-raii.hpp"

#include "killsense/profile.hpp"

#include <string>
#include <vector>

namespace killsense::obsplugin {

struct SourceInfo {
	std::string uuid;
	std::string name;
	std::string typeId; // unversioned id, e.g. "game_capture", "scene"
	bool isScene = false;
};

// Video-capable inputs and scenes, sorted: captures first, then others, then scenes.
std::vector<SourceInfo> listVideoSources();

// Resolves by UUID first, then by name. Returns an owned strong reference.
SourcePtr findSource(const std::string &uuid, const std::string &name);

struct CaptureTarget {
	bool known = false;
	std::string executable; // e.g. "Game.exe"
	std::string windowTitle;
	std::string windowClass;
	std::string captureMode; // game_capture: "window", "any_fullscreen", "hotkey"
};

// Reads the "window" setting of game_capture / window_capture sources
// (format "title:class:executable", ':' encoded as "#3A" and '#' as "#22").
CaptureTarget readCaptureTarget(obs_source_t *source);

struct AutoDetectResult {
	ProfilePtr profile; // nullptr if nothing matched
	std::string message;
};

AutoDetectResult autoDetectProfile(obs_source_t *source, const std::vector<ProfilePtr> &profiles);

} // namespace killsense::obsplugin
