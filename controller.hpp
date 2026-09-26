// REALM KillSense - plugin controller.
//
// Owns every runtime component and defines thread ownership:
//   UI thread (Qt dock)         -> Controller public API
//   OBS graphics thread         -> FrameSampler::tick (capture only)
//   KillSense analysis thread   -> AnalysisWorker (detectors, OCR, fusion)
//   KillSense dispatch thread   -> EventBus (outputs: obs-websocket, JSONL)
//   OBS hotkey / websocket thr. -> testKill / resetSession / toggleDetection
// All public methods are thread-safe.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "obs/frame-sampler.hpp"
#include "obs/obs-sources.hpp"
#include "outputs/websocket-vendor-output.hpp"

#include "killsense/config.hpp"
#include "killsense/event-bus.hpp"
#include "killsense/profile.hpp"
#include "killsense/session.hpp"
#include "killsense/worker.hpp"

#include <obs.h>
#include <util/platform.h>

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace killsense::obsplugin {

struct ControllerSnapshot {
	std::string sourceName;
	bool sourceSelected = false;
	std::string profileId;
	std::string profileName;
	bool detecting = false;
	std::string detectorState;
	SessionStats session;
	WorkerStats worker;
	SamplerStats sampler;
	double cpuPercent = -1.0;
	bool websocketAvailable = false;
	bool debugEnabled = false;
	std::string gpuState;
	std::string gamertag;
	bool overlayPopups = true;
	// Non-empty when the active profile needs player setup (gamertag or a
	// captured name plate) before it can count YOUR kills.
	std::string setupHint;
};

// A per-user image the active profile wants (e.g. your own name plate).
struct UserTemplateSlot {
	std::string name;       // file name (without .png)
	std::string detectorId; // template detector that uses it
	std::string regionId;   // HUD region to capture it from
	bool captured = false;
};

class Controller {
public:
	static std::shared_ptr<Controller> create();
	~Controller();
	Controller(const Controller &) = delete;
	Controller &operator=(const Controller &) = delete;

	// ---- lifecycle (called from plugin-main.cpp) ----
	void registerHotkeys();
	void postLoad();          // obs-websocket vendor registration
	void onFinishedLoading(); // resolve saved source, optional auto start
	void onExit();            // stop everything that talks to other modules
	void shutdown();          // final teardown (module unload); idempotent

	// ---- sources / profiles ----
	std::vector<SourceInfo> videoSources() const;
	void selectSource(const std::string &uuid, const std::string &name);
	std::vector<ProfilePtr> profiles() const;
	std::vector<ProfileLoadIssue> profileIssues() const;
	void reloadProfiles();
	bool selectProfile(const std::string &id);
	std::string autoDetectProfile(); // returns a user-facing message

	// ---- detection ----
	bool startDetection(std::string *error = nullptr);
	void stopDetection();
	void toggleDetection();
	bool detecting() const;
	void testKill();
	void resetSession();

	// ---- personalisation ----
	std::vector<UserTemplateSlot> userTemplateSlots() const;
	// Saves `rgba` (reference scale) as the user's image for `name` in the
	// active profile and reloads it into the pipeline.
	bool saveUserTemplate(const std::string &name, const Image &rgba, std::string *error = nullptr);
	std::filesystem::path userTemplatesDir() const { return userTemplatesDir_; }
	// Pushes the current counters to the configured OBS text source (any thread).
	void refreshCounterSource();
	// Overlay pop-up text on/off: saved and broadcast to overlays live.
	void setOverlayPopups(bool enabled);

	// ---- debug ----
	void setDebugEnabled(bool enabled);
	bool debugSnapshot(DebugSnapshot &out) const;

	// ---- settings ----
	AppConfig config() const;
	void applyConfig(const AppConfig &cfg); // applies live where possible and saves
	void saveConfig();

	ControllerSnapshot snapshot() const;
	std::string statusJson() const;

	std::filesystem::path builtinProfilesDir() const { return builtinProfilesDir_; }
	std::filesystem::path userProfilesDir() const { return userProfilesDir_; }
	std::filesystem::path configFile() const { return configFile_; }

private:
	Controller();
	void init();
	void rebuildActiveProfile();  // requires mutex_
	void pushProfileToPipeline(); // requires mutex_
	void onDetections(const std::vector<DetectedEvent> &events, const ProfilePtr &profile);
	void publish(KillEvent ev);
	void updateCounterSourceOnUiThread();
	void loadHotkeys();
	void storeHotkeys(); // requires mutex_

	static void hotkeyTestKill(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
	static void hotkeyToggle(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);

	std::filesystem::path configFile_;
	std::filesystem::path builtinProfilesDir_;
	std::filesystem::path userProfilesDir_;
	std::filesystem::path userTemplatesDir_;

	mutable std::recursive_mutex mutex_; // guards config_, activeProfile_, lifecycle
	AppConfig config_;
	ProfilePtr activeProfile_; // selected profile with user overrides applied
	ProfilePtr baseProfile_;   // selected profile as loaded (before personalisation)
	std::string setupHint_;
	bool detecting_ = false;
	bool exiting_ = false;
	bool shutDown_ = false;

	ProfileRegistry registry_;
	SessionTracker session_;
	EventBus bus_;
	FramePool pool_{4};
	std::unique_ptr<AnalysisWorker> worker_;
	std::unique_ptr<FrameSampler> sampler_;
	std::shared_ptr<WebsocketVendorOutput> websocket_;
	std::shared_ptr<JsonlFileOutput> jsonl_;

	obs_hotkey_id hotkeyTestKill_ = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id hotkeyToggle_ = OBS_INVALID_HOTKEY_ID;
	os_cpu_usage_info_t *cpuInfo_ = nullptr;
	mutable std::mutex cpuMutex_;
	mutable double lastCpu_ = -1.0;
	mutable uint64_t lastCpuQueryNs_ = 0;
};

// Global access for the dock/dialogs (UI thread). Returns nullptr after unload.
std::shared_ptr<Controller> controllerInstance();
void setControllerInstance(std::shared_ptr<Controller> c);

} // namespace killsense::obsplugin
