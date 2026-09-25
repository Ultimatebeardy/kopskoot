// SPDX-License-Identifier: GPL-2.0-or-later
#include "controller.hpp"

#include "obs/obs-raii.hpp"

#include "killsense/fsutil.hpp"
#include "killsense/logging.hpp"
#include "killsense/ocr.hpp"
#include "killsense/version.hpp"

#include <obs-module.h>

#include <nlohmann/json.hpp>

#include <algorithm>

namespace killsense::obsplugin {

namespace {
std::mutex g_instanceMutex;
std::shared_ptr<Controller> g_instance;
} // namespace

std::shared_ptr<Controller> controllerInstance()
{
	std::lock_guard<std::mutex> lock(g_instanceMutex);
	return g_instance;
}

void setControllerInstance(std::shared_ptr<Controller> c)
{
	std::lock_guard<std::mutex> lock(g_instanceMutex);
	g_instance = std::move(c);
}

std::shared_ptr<Controller> Controller::create()
{
	std::shared_ptr<Controller> c(new Controller());
	c->init();
	return c;
}

Controller::Controller() = default;

Controller::~Controller()
{
	shutdown();
}

void Controller::init()
{
	BStr cfg(obs_module_config_path("config.json"));
	BStr userProfiles(obs_module_config_path("profiles"));
	BStr builtin(obs_module_file("profiles"));
	configFile_ = pathFromUtf8(toString(cfg));
	userProfilesDir_ = pathFromUtf8(toString(userProfiles));
	builtinProfilesDir_ = pathFromUtf8(toString(builtin));

	std::error_code ec;
	if (!userProfilesDir_.empty())
		std::filesystem::create_directories(userProfilesDir_, ec);

	auto loaded = loadConfigFile(configFile_);
	for (const auto &w : loaded.warnings)
		KS_LOG_WARN("%s", w.c_str());
	config_ = loaded.config;
	setLogLevel(config_.logLevel);
	KS_LOG_INFO("config: %s (%s)", pathToUtf8(configFile_).c_str(), loaded.loadedFromFile ? "loaded" : "defaults");

	registry_.loadDirectories({builtinProfilesDir_}, {userProfilesDir_});

	jsonl_ = std::make_shared<JsonlFileOutput>(config_.jsonlPath);
	jsonl_->setEnabled(config_.outputJsonl && !config_.jsonlPath.empty());
	WebsocketVendorOutput::Handlers handlers;
	handlers.getStatusJson = [this] {
		return statusJson();
	};
	handlers.testKill = [this] {
		testKill();
	};
	handlers.resetSession = [this] {
		resetSession();
	};
	handlers.startDetection = [this](const std::string &sourceName, const std::string &profileId) {
		if (!profileId.empty() && !selectProfile(profileId))
			return std::string("profile not found: ") + profileId;
		if (!sourceName.empty()) {
			SourcePtr source = findSource("", sourceName);
			if (!source)
				return std::string("source not found: ") + sourceName;
			const char *uuid = obs_source_get_uuid(source.get());
			selectSource(uuid ? uuid : "", sourceName);
		}
		std::string err;
		startDetection(&err);
		return err;
	};
	handlers.stopDetection = [this] {
		stopDetection();
	};
	websocket_ = std::make_shared<WebsocketVendorOutput>(std::move(handlers));
	websocket_->setEnabled(config_.outputWebsocketVendor);
	bus_.addOutput(websocket_);
	bus_.addOutput(jsonl_);
	bus_.start();

	const std::string ocrLanguage = config_.ocrLanguage;
	worker_ = std::make_unique<AnalysisWorker>(
		pool_, [ocrLanguage] { return createPlatformOcrEngine(ocrLanguage); },
		[this](const std::vector<DetectedEvent> &events, const ProfilePtr &profile) {
			onDetections(events, profile);
		});
	sampler_ = std::make_unique<FrameSampler>(pool_, *worker_);

	cpuInfo_ = os_cpu_usage_info_start();

	std::lock_guard<std::recursive_mutex> lock(mutex_);
	rebuildActiveProfile();
	pushProfileToPipeline();
}

void Controller::registerHotkeys()
{
	hotkeyTestKill_ = obs_hotkey_register_frontend("realm_killsense.test_kill",
						       obs_module_text("KillSense.Hotkey.TestKill"),
						       &Controller::hotkeyTestKill, this);
	hotkeyToggle_ = obs_hotkey_register_frontend("realm_killsense.toggle_detection",
						     obs_module_text("KillSense.Hotkey.ToggleDetection"),
						     &Controller::hotkeyToggle, this);
	loadHotkeys();
}

void Controller::hotkeyTestKill(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (pressed)
		static_cast<Controller *>(data)->testKill();
}

void Controller::hotkeyToggle(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (pressed)
		static_cast<Controller *>(data)->toggleDetection();
}

void Controller::loadHotkeys()
{
	std::string json;
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		json = config_.hotkeysJson;
	}
	if (json.empty())
		return;
	DataPtr data(obs_data_create_from_json(json.c_str()));
	if (!data)
		return;
	DataArrayPtr test(obs_data_get_array(data.get(), "test_kill"));
	DataArrayPtr toggle(obs_data_get_array(data.get(), "toggle_detection"));
	if (test && hotkeyTestKill_ != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_load(hotkeyTestKill_, test.get());
	if (toggle && hotkeyToggle_ != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_load(hotkeyToggle_, toggle.get());
}

void Controller::storeHotkeys()
{
	if (hotkeyTestKill_ == OBS_INVALID_HOTKEY_ID)
		return;
	DataPtr data(obs_data_create());
	DataArrayPtr test(obs_hotkey_save(hotkeyTestKill_));
	DataArrayPtr toggle(obs_hotkey_save(hotkeyToggle_));
	obs_data_set_array(data.get(), "test_kill", test.get());
	obs_data_set_array(data.get(), "toggle_detection", toggle.get());
	const char *json = obs_data_get_json(data.get());
	config_.hotkeysJson = json ? json : "";
}

void Controller::postLoad()
{
	websocket_->registerVendor();
}

void Controller::onFinishedLoading()
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	if (!config_.sourceUuid.empty() || !config_.sourceName.empty()) {
		SourcePtr source = findSource(config_.sourceUuid, config_.sourceName);
		sampler_->setSource(source.get());
		if (!source)
			KS_LOG_WARN("saved source '%s' not found in this scene collection", config_.sourceName.c_str());
	}
	if (config_.autoStart) {
		std::string err;
		if (!startDetection(&err))
			KS_LOG_WARN("auto start failed: %s", err.c_str());
	}
}

void Controller::onExit()
{
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		if (exiting_)
			return;
		exiting_ = true;
		storeHotkeys();
		saveConfig();
	}
	stopDetection();
	// Not holding mutex_: obs-websocket may be executing one of our request
	// callbacks (which takes mutex_) while we unregister.
	websocket_->unregister();
}

void Controller::shutdown()
{
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		if (shutDown_)
			return;
		shutDown_ = true;
	}
	if (sampler_)
		sampler_->stop(); // releases GPU resources inside the graphics context
	if (worker_)
		worker_->stop();
	bus_.stop();
	if (websocket_)
		websocket_->setEnabled(false);
	if (hotkeyTestKill_ != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(hotkeyTestKill_);
	if (hotkeyToggle_ != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(hotkeyToggle_);
	hotkeyTestKill_ = hotkeyToggle_ = OBS_INVALID_HOTKEY_ID;
	if (cpuInfo_) {
		os_cpu_usage_info_destroy(cpuInfo_);
		cpuInfo_ = nullptr;
	}
	sampler_.reset();
	worker_.reset();
	KS_LOG_INFO("shut down cleanly");
}

std::vector<SourceInfo> Controller::videoSources() const
{
	return listVideoSources();
}

void Controller::selectSource(const std::string &uuid, const std::string &name)
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	config_.sourceUuid = uuid;
	config_.sourceName = name;
	SourcePtr source = findSource(uuid, name);
	sampler_->setSource(source.get());
	worker_->requestTemporalReset();
	saveConfig();
}

std::vector<ProfilePtr> Controller::profiles() const
{
	return registry_.list();
}

std::vector<ProfileLoadIssue> Controller::profileIssues() const
{
	return registry_.issues();
}

void Controller::reloadProfiles()
{
	registry_.loadDirectories({builtinProfilesDir_}, {userProfilesDir_});
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	rebuildActiveProfile();
	pushProfileToPipeline();
}

bool Controller::selectProfile(const std::string &id)
{
	if (!registry_.get(id))
		return false;
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	config_.profileId = id;
	rebuildActiveProfile();
	pushProfileToPipeline();
	saveConfig();
	return true;
}

std::string Controller::autoDetectProfile()
{
	SourcePtr source;
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		source = findSource(config_.sourceUuid, config_.sourceName);
	}
	const AutoDetectResult r = obsplugin::autoDetectProfile(source.get(), registry_.list());
	if (r.profile)
		selectProfile(r.profile->id);
	return r.message;
}

void Controller::rebuildActiveProfile()
{
	ProfilePtr base = registry_.get(config_.profileId);
	if (!base) {
		base = registry_.get("generic");
		if (base)
			KS_LOG_WARN("profile '%s' not found, falling back to 'generic'", config_.profileId.c_str());
	}
	if (!base) {
		activeProfile_.reset();
		KS_LOG_ERROR("no usable profile found (is the data/profiles directory installed?)");
		return;
	}
	ProfileOverrides ov;
	ov.threshold = config_.thresholdOverride;
	ov.cooldownMs = config_.cooldownOverrideMs;
	if (config_.customRoiEnabled)
		ov.primaryRegion = config_.customRoi;
	activeProfile_ = applyOverrides(*base, ov);
}

void Controller::pushProfileToPipeline()
{
	const PresetSettings preset = config_.effectivePreset();
	worker_->setProfile(activeProfile_, preset);
	sampler_->setProfile(activeProfile_, preset);
}

bool Controller::startDetection(std::string *error)
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	if (shutDown_ || exiting_) {
		if (error)
			*error = "OBS is shutting down";
		return false;
	}
	if (detecting_)
		return true;
	if (!activeProfile_) {
		if (error)
			*error = "no profile loaded";
		return false;
	}
	SourcePtr source = findSource(config_.sourceUuid, config_.sourceName);
	if (!source) {
		if (error)
			*error = "select a video source first";
		return false;
	}
	sampler_->setSource(source.get());
	pushProfileToPipeline();
	worker_->start();
	sampler_->start();
	detecting_ = true;
	KS_LOG_INFO("detection started (source '%s', profile '%s')", config_.sourceName.c_str(),
		    activeProfile_->id.c_str());
	return true;
}

void Controller::stopDetection()
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	if (!detecting_)
		return;
	sampler_->stop(); // first: no new frames
	worker_->stop();  // then join the analysis thread
	detecting_ = false;
	KS_LOG_INFO("detection stopped");
}

void Controller::toggleDetection()
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	if (detecting_)
		stopDetection();
	else
		startDetection();
}

bool Controller::detecting() const
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	return detecting_;
}

void Controller::onDetections(const std::vector<DetectedEvent> &events, const ProfilePtr &profile)
{
	// Worker thread. Must not take mutex_ (stopDetection joins this thread
	// while holding it).
	for (const auto &det : events) {
		KillEvent ev = makeKillEvent(det, profile.get(), "vision");
		publish(std::move(ev));
	}
}

void Controller::publish(KillEvent ev)
{
	session_.record(ev);
	KS_LOG_INFO("event %s (confidence %.2f, session kills %lld, streak %lld)%s", eventTypeId(ev.type),
		    ev.confidence, static_cast<long long>(ev.sessionKills), static_cast<long long>(ev.killStreak),
		    ev.test ? " [test]" : "");
	bus_.publish(ev);
}

void Controller::testKill()
{
	KillEvent ev;
	ev.type = EventType::Kill;
	ProfilePtr p;
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		p = activeProfile_;
	}
	ev.profile = p ? p->id : "none";
	ev.game = p ? p->displayName : "unknown";
	ev.target = "test";
	ev.confidence = 1.0;
	ev.detector = "test";
	ev.test = true;
	ev.reason = "Test Kill Event";
	publish(std::move(ev));
}

void Controller::resetSession()
{
	ProfilePtr p;
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		p = activeProfile_;
	}
	KillEvent ev = session_.reset(p ? p->id : "none", p ? p->displayName : "unknown");
	if (worker_)
		worker_->requestTemporalReset();
	bus_.publish(ev);
	KS_LOG_INFO("session reset");
}

void Controller::setDebugEnabled(bool enabled)
{
	if (worker_)
		worker_->setDebugEnabled(enabled);
}

bool Controller::debugSnapshot(DebugSnapshot &out) const
{
	return worker_ && worker_->debugSnapshot(out);
}

AppConfig Controller::config() const
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	return config_;
}

void Controller::applyConfig(const AppConfig &cfg)
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	const bool ocrChanged = cfg.ocrLanguage != config_.ocrLanguage;
	const std::string hotkeys = config_.hotkeysJson;
	config_ = cfg;
	config_.hotkeysJson = hotkeys; // owned by the hotkey system
	setLogLevel(config_.logLevel);
	websocket_->setEnabled(config_.outputWebsocketVendor);
	jsonl_->setPath(config_.jsonlPath);
	jsonl_->setEnabled(config_.outputJsonl && !config_.jsonlPath.empty());
	rebuildActiveProfile();
	pushProfileToPipeline();
	if (ocrChanged)
		KS_LOG_INFO("OCR language change takes effect after OBS restarts");
	saveConfig();
}

void Controller::saveConfig()
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	if (configFile_.empty())
		return;
	if (!saveConfigFile(configFile_, config_))
		KS_LOG_WARN("could not save %s", pathToUtf8(configFile_).c_str());
}

ControllerSnapshot Controller::snapshot() const
{
	ControllerSnapshot s;
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		s.sourceName = config_.sourceName;
		s.sourceSelected = !config_.sourceUuid.empty() || !config_.sourceName.empty();
		s.profileId = activeProfile_ ? activeProfile_->id : "";
		s.profileName = activeProfile_ ? activeProfile_->displayName : "(none)";
		s.detecting = detecting_;
	}
	s.session = session_.stats();
	if (worker_)
		s.worker = worker_->stats();
	if (sampler_)
		s.sampler = sampler_->stats();
	s.websocketAvailable = websocket_ && websocket_->available();
	s.debugEnabled = worker_ && worker_->debugEnabled();
	s.gpuState = "Not used (ONNX inference planned - Milestone 4)";

	if (!s.detecting)
		s.detectorState = "Stopped";
	else if (!s.sampler.sourceAvailable)
		s.detectorState = "Waiting for source (not found or 0x0)";
	else if (!s.worker.unavailableDetectors.empty())
		s.detectorState = "Running (" + std::to_string(s.worker.unavailableDetectors.size()) +
				  " detector(s) unavailable)";
	else
		s.detectorState = "Running";

	{
		std::lock_guard<std::mutex> lock(cpuMutex_);
		const uint64_t now = os_gettime_ns();
		if (cpuInfo_ && now - lastCpuQueryNs_ > 1000000000ULL) {
			lastCpu_ = os_cpu_usage_info_query(cpuInfo_);
			lastCpuQueryNs_ = now;
		}
		s.cpuPercent = lastCpu_;
	}
	return s;
}

std::string Controller::statusJson() const
{
	const ControllerSnapshot s = snapshot();
	nlohmann::ordered_json j;
	j["schemaVersion"] = kEventSchemaVersion;
	j["version"] = kCoreVersion;
	j["detecting"] = s.detecting;
	j["detectorState"] = s.detectorState;
	j["source"] = s.sourceName;
	j["profile"] = s.profileId;
	j["game"] = s.profileName;
	j["sessionKills"] = s.session.kills;
	j["killStreak"] = s.session.streak;
	j["bestStreak"] = s.session.bestStreak;
	j["sessionDeaths"] = s.session.deaths;
	j["analysisFps"] = s.worker.analysisFps;
	j["avgProcessingMs"] = s.worker.avgProcessingMs;
	j["droppedFrames"] = s.worker.framesDropped + s.sampler.skippedWorkerBusy;
	j["ocrEngine"] = s.worker.ocrEngine;
	j["ocrAvailable"] = s.worker.ocrAvailable;
	if (s.session.lastEvent)
		j["lastEvent"] = nlohmann::json::parse(toJson(*s.session.lastEvent, false));
	return j.dump();
}

} // namespace killsense::obsplugin
