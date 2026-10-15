/*
KopSkoot - OBS Studio plugin entry point.
Copyright (C) 2026 Ultimate Beardy / The Realm

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "controller.hpp"
#include "ui/killsense-dock.hpp"

#include "killsense/logging.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

#include <QMainWindow>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "KopSkoot: local, vision-based kill detection for stream automation.";
}

namespace {

constexpr const char *kDockId = "realm-killsense-dock";

void installLogBridge()
{
	killsense::setLogSink([](killsense::LogLevel level, const std::string &msg) {
		switch (level) {
		case killsense::LogLevel::Error:
			obs_log(LOG_ERROR, "%s", msg.c_str());
			break;
		case killsense::LogLevel::Warning:
			obs_log(LOG_WARNING, "%s", msg.c_str());
			break;
		case killsense::LogLevel::Info:
			obs_log(LOG_INFO, "%s", msg.c_str());
			break;
		default:
			// OBS hides LOG_DEBUG in release builds; when the user selects the
			// KillSense debug/trace level we want the lines to be visible.
			obs_log(LOG_INFO, "[debug] %s", msg.c_str());
			break;
		}
	});
}

void frontendEvent(enum obs_frontend_event event, void *)
{
	auto controller = killsense::obsplugin::controllerInstance();
	if (!controller)
		return;
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		controller->onFinishedLoading();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		controller->onExit();
		break;
	default:
		break;
	}
}

} // namespace

bool obs_module_load(void)
{
	installLogBridge();
	try {
		auto controller = killsense::obsplugin::Controller::create();
		killsense::obsplugin::setControllerInstance(controller);
		controller->registerHotkeys();

		auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		auto *dock = new killsense::obsplugin::KillSenseDock(controller, mainWindow);
		if (!obs_frontend_add_dock_by_id(kDockId, obs_module_text("KillSense.DockTitle"), dock)) {
			obs_log(LOG_WARNING, "could not add dock (id already in use?)");
			delete dock;
		}
		obs_frontend_add_event_callback(frontendEvent, nullptr);
	} catch (const std::exception &e) {
		obs_log(LOG_ERROR, "failed to initialise: %s", e.what());
		killsense::obsplugin::setControllerInstance(nullptr);
		return false;
	}
	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_post_load(void)
{
	if (auto controller = killsense::obsplugin::controllerInstance())
		controller->postLoad();
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontendEvent, nullptr);
	auto controller = killsense::obsplugin::controllerInstance();
	killsense::obsplugin::setControllerInstance(nullptr);
	if (controller)
		controller->shutdown();
	controller.reset();
	killsense::setLogSink(nullptr);
	obs_log(LOG_INFO, "plugin unloaded");
}
