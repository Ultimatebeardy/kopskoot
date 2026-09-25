// SPDX-License-Identifier: GPL-2.0-or-later
#include "websocket-vendor-output.hpp"

#include "../obs/obs-raii.hpp"

#include "killsense/logging.hpp"
#include "killsense/version.hpp"

#include <obs.h>

#include <cassert>
#include <cstring>
// Vendored from obsproject/obs-websocket (lib/obs-websocket-api.h, GPL-2.0+).
// Header-only; talks to obs-websocket through libobs proc handlers, so there is
// no link-time dependency and the plugin still loads without obs-websocket.
#include <obs-websocket-api.h>

namespace killsense::obsplugin {

struct VendorCallbacks {
	static void getStatus(obs_data_t *, obs_data_t *response, void *priv)
	{
		auto *self = static_cast<WebsocketVendorOutput *>(priv);
		if (self->shutdown_.load() || !self->handlers_.getStatusJson)
			return;
		try {
			DataPtr status(obs_data_create_from_json(self->handlers_.getStatusJson().c_str()));
			if (status)
				obs_data_apply(response, status.get());
		} catch (...) {
			obs_data_set_string(response, "error", "internal error");
		}
	}

	static void testKill(obs_data_t *, obs_data_t *response, void *priv)
	{
		auto *self = static_cast<WebsocketVendorOutput *>(priv);
		if (self->shutdown_.load() || !self->handlers_.testKill)
			return;
		try {
			self->handlers_.testKill();
			obs_data_set_bool(response, "ok", true);
		} catch (...) {
			obs_data_set_bool(response, "ok", false);
		}
	}

	static void resetSession(obs_data_t *, obs_data_t *response, void *priv)
	{
		auto *self = static_cast<WebsocketVendorOutput *>(priv);
		if (self->shutdown_.load() || !self->handlers_.resetSession)
			return;
		try {
			self->handlers_.resetSession();
			obs_data_set_bool(response, "ok", true);
		} catch (...) {
			obs_data_set_bool(response, "ok", false);
		}
	}

	static void startDetection(obs_data_t *request, obs_data_t *response, void *priv)
	{
		auto *self = static_cast<WebsocketVendorOutput *>(priv);
		if (self->shutdown_.load() || !self->handlers_.startDetection)
			return;
		try {
			const char *name = request ? obs_data_get_string(request, "sourceName") : "";
			const char *profile = request ? obs_data_get_string(request, "profileId") : "";
			const std::string err =
				self->handlers_.startDetection(name ? name : "", profile ? profile : "");
			obs_data_set_bool(response, "ok", err.empty());
			if (!err.empty())
				obs_data_set_string(response, "error", err.c_str());
		} catch (...) {
			obs_data_set_bool(response, "ok", false);
		}
	}

	static void stopDetection(obs_data_t *, obs_data_t *response, void *priv)
	{
		auto *self = static_cast<WebsocketVendorOutput *>(priv);
		if (self->shutdown_.load() || !self->handlers_.stopDetection)
			return;
		try {
			self->handlers_.stopDetection();
			obs_data_set_bool(response, "ok", true);
		} catch (...) {
			obs_data_set_bool(response, "ok", false);
		}
	}
};

WebsocketVendorOutput::WebsocketVendorOutput(Handlers handlers) : handlers_(std::move(handlers)) {}

WebsocketVendorOutput::~WebsocketVendorOutput()
{
	// Do not call into obs-websocket here: at module unload it may already be
	// gone. unregister() must have been called at OBS_FRONTEND_EVENT_EXIT.
	shutdown_.store(true);
}

bool WebsocketVendorOutput::registerVendor()
{
	if (vendor_)
		return true;
	apiVersion_ = obs_websocket_get_api_version();
	if (apiVersion_ == 0) {
		KS_LOG_INFO("obs-websocket not available; vendor events disabled");
		return false;
	}
	vendor_ = obs_websocket_register_vendor(kVendorName);
	if (!vendor_) {
		KS_LOG_WARN("obs-websocket vendor registration failed");
		return false;
	}
	obs_websocket_vendor_register_request(vendor_, "GetStatus", &VendorCallbacks::getStatus, this);
	obs_websocket_vendor_register_request(vendor_, "TestKill", &VendorCallbacks::testKill, this);
	obs_websocket_vendor_register_request(vendor_, "ResetSession", &VendorCallbacks::resetSession, this);
	obs_websocket_vendor_register_request(vendor_, "StartDetection", &VendorCallbacks::startDetection, this);
	obs_websocket_vendor_register_request(vendor_, "StopDetection", &VendorCallbacks::stopDetection, this);
	KS_LOG_INFO("obs-websocket vendor '%s' registered (API v%u)", kVendorName, apiVersion_);
	return true;
}

void WebsocketVendorOutput::unregister()
{
	if (shutdown_.exchange(true))
		return;
	std::lock_guard<std::mutex> lock(emitMutex_);
	if (vendor_) {
		obs_websocket_vendor_unregister_request(vendor_, "GetStatus");
		obs_websocket_vendor_unregister_request(vendor_, "TestKill");
		obs_websocket_vendor_unregister_request(vendor_, "ResetSession");
		obs_websocket_vendor_unregister_request(vendor_, "StartDetection");
		obs_websocket_vendor_unregister_request(vendor_, "StopDetection");
	}
	vendor_ = nullptr;
}

void WebsocketVendorOutput::deliver(const KillEvent &ev)
{
	std::lock_guard<std::mutex> lock(emitMutex_);
	if (!vendor_ || shutdown_.load())
		return;
	DataPtr data(obs_data_create_from_json(toJson(ev, false).c_str()));
	if (!data)
		return;
	if (!obs_websocket_vendor_emit_event(vendor_, eventTypeVendorName(ev.type), data.get()))
		KS_LOG_DEBUG("obs-websocket vendor event '%s' was not emitted", eventTypeVendorName(ev.type));
}

} // namespace killsense::obsplugin
