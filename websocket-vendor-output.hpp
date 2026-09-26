// REALM KillSense - obs-websocket (v5) vendor integration.
//
// Vendor name: "RealmKillSense". Events are emitted as obs-websocket
// VendorEvent messages (eventType = "Kill", "Death", "Headshot", ...,
// "SessionReset"; eventData = the KillSense event JSON). Clients such as
// Streamer.bot subscribe to the Vendors event category.
//
// Vendor requests (via CallVendorRequest):
//   GetStatus     -> session counters + detector state
//   TestKill      -> emits a test kill event
//   ResetSession  -> resets counters and emits SessionReset
//   StartDetection {"sourceName"?: string, "profileId"?: string}
//                 -> optionally selects source/profile, then starts detection
//   StopDetection -> stops detection
//
// The vendor API is only usable after obs_module_post_load() and must be
// released before obs-websocket unloads (we do it at OBS_FRONTEND_EVENT_EXIT).
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "killsense/event-bus.hpp"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>

namespace killsense::obsplugin {

class WebsocketVendorOutput final : public IEventOutput {
public:
	struct Handlers {
		std::function<std::string()> getStatusJson;
		std::function<void()> testKill;
		std::function<void()> resetSession;
		// returns an error message or "" on success
		std::function<std::string(const std::string &sourceName, const std::string &profileId)> startDetection;
		std::function<void()> stopDetection;
	};

	explicit WebsocketVendorOutput(Handlers handlers);
	~WebsocketVendorOutput() override;

	// Call from obs_module_post_load(). Returns false if obs-websocket is absent.
	bool registerVendor();
	// Call at OBS_FRONTEND_EVENT_EXIT (while obs-websocket is still loaded).
	void unregister();

	bool available() const { return vendor_ != nullptr && !shutdown_.load(); }
	unsigned int apiVersion() const { return apiVersion_; }

	void setEnabled(bool enabled) { enabled_.store(enabled); }
	const char *name() const override { return "obs-websocket-vendor"; }
	bool enabled() const override { return enabled_.load() && available(); }
	void deliver(const KillEvent &ev) override;
	// Emits a non-kill vendor event (e.g. "OverlaySettings") with a JSON object.
	void emitCustom(const char *eventType, const std::string &json);

private:
	friend struct VendorCallbacks;

	Handlers handlers_;
	void *vendor_ = nullptr; // obs_websocket_vendor
	unsigned int apiVersion_ = 0;
	std::atomic<bool> enabled_{true};
	std::atomic<bool> shutdown_{false};
	std::mutex emitMutex_;
};

} // namespace killsense::obsplugin
