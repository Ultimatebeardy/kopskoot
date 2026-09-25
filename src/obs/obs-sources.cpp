// SPDX-License-Identifier: GPL-2.0-or-later
#include "obs-sources.hpp"

#include <algorithm>
#include <cctype>

namespace killsense::obsplugin {

namespace {

bool enumProc(void *param, obs_source_t *source)
{
	auto *out = static_cast<std::vector<SourceInfo> *>(param);
	const uint32_t flags = obs_source_get_output_flags(source);
	if (!(flags & OBS_SOURCE_VIDEO))
		return true;
	SourceInfo info;
	const char *uuid = obs_source_get_uuid(source);
	const char *name = obs_source_get_name(source);
	const char *id = obs_source_get_unversioned_id(source);
	info.uuid = uuid ? uuid : "";
	info.name = name ? name : "";
	info.typeId = id ? id : "";
	info.isScene = obs_source_get_type(source) == OBS_SOURCE_TYPE_SCENE;
	if (!info.name.empty())
		out->push_back(std::move(info));
	return true;
}

int rank(const SourceInfo &s)
{
	if (s.typeId == "game_capture")
		return 0;
	if (s.typeId == "window_capture" || s.typeId == "monitor_capture" || s.typeId == "xcomposite_input" ||
	    s.typeId == "pipewire-screen-capture-source" || s.typeId == "display_capture")
		return 1;
	if (s.isScene)
		return 3;
	return 2;
}

std::string lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(),
		       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}

std::string decodeSegment(std::string s)
{
	auto replaceAll = [&s](const std::string &from, const std::string &to) {
		size_t pos = 0;
		while ((pos = s.find(from, pos)) != std::string::npos) {
			s.replace(pos, from.size(), to);
			pos += to.size();
		}
	};
	replaceAll("#3A", ":");
	replaceAll("#22", "#");
	return s;
}

} // namespace

std::vector<SourceInfo> listVideoSources()
{
	std::vector<SourceInfo> out;
	obs_enum_sources(enumProc, &out); // inputs
	obs_enum_scenes(enumProc, &out);  // scenes (handy for testing overlays)
	std::stable_sort(out.begin(), out.end(), [](const SourceInfo &a, const SourceInfo &b) {
		if (rank(a) != rank(b))
			return rank(a) < rank(b);
		return lower(a.name) < lower(b.name);
	});
	return out;
}

SourcePtr findSource(const std::string &uuid, const std::string &name)
{
	if (!uuid.empty()) {
		if (obs_source_t *s = obs_get_source_by_uuid(uuid.c_str()))
			return SourcePtr(s);
	}
	if (!name.empty()) {
		if (obs_source_t *s = obs_get_source_by_name(name.c_str()))
			return SourcePtr(s);
	}
	return nullptr;
}

CaptureTarget readCaptureTarget(obs_source_t *source)
{
	CaptureTarget t;
	if (!source)
		return t;
	const char *id = obs_source_get_unversioned_id(source);
	if (!id || (std::string(id) != "game_capture" && std::string(id) != "window_capture"))
		return t;
	DataPtr settings(obs_source_get_settings(source));
	if (!settings)
		return t;
	const char *mode = obs_data_get_string(settings.get(), "capture_mode");
	t.captureMode = mode ? mode : "";
	const char *window = obs_data_get_string(settings.get(), "window");
	if (!window || !*window)
		return t;
	const std::string w = window;
	const size_t a = w.find(':');
	const size_t b = a == std::string::npos ? std::string::npos : w.find(':', a + 1);
	if (a == std::string::npos || b == std::string::npos)
		return t;
	t.windowTitle = decodeSegment(w.substr(0, a));
	t.windowClass = decodeSegment(w.substr(a + 1, b - a - 1));
	t.executable = decodeSegment(w.substr(b + 1));
	t.known = !t.executable.empty() || !t.windowTitle.empty();
	return t;
}

AutoDetectResult autoDetectProfile(obs_source_t *source, const std::vector<ProfilePtr> &profiles)
{
	AutoDetectResult r;
	if (!source) {
		r.message = "No source selected.";
		return r;
	}
	const CaptureTarget target = readCaptureTarget(source);
	const std::string sourceName = lower(obs_source_get_name(source) ? obs_source_get_name(source) : "");
	const std::string exe = lower(target.executable);
	const std::string title = lower(target.windowTitle);

	for (const auto &p : profiles) {
		if (!p || p->generic)
			continue;
		for (const auto &pn : p->match.processNames) {
			if (!exe.empty() && lower(pn) == exe) {
				r.profile = p;
				r.message = "Matched executable '" + target.executable + "'.";
				return r;
			}
		}
		for (const auto &tc : p->match.windowTitleContains) {
			if (!title.empty() && !tc.empty() && title.find(lower(tc)) != std::string::npos) {
				r.profile = p;
				r.message = "Matched window title '" + target.windowTitle + "'.";
				return r;
			}
		}
		for (const auto &sn : p->match.sourceNameContains) {
			if (!sn.empty() && sourceName.find(lower(sn)) != std::string::npos) {
				r.profile = p;
				r.message = "Matched source name.";
				return r;
			}
		}
	}
	for (const auto &p : profiles)
		if (p && p->generic) {
			r.profile = p;
			break;
		}
	if (!target.known) {
		r.message =
			target.captureMode == "any_fullscreen"
				? "Game Capture is set to 'any fullscreen application', so the game cannot be identified. Using the generic profile."
				: "The source does not identify a game window. Using the generic profile.";
	} else {
		r.message = "No profile matches '" +
			    (target.executable.empty() ? target.windowTitle : target.executable) +
			    "'. Using the generic profile.";
	}
	return r;
}

} // namespace killsense::obsplugin
