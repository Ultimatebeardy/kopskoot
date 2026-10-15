// KopSkoot - OBS frame sampler (graphics thread side).
//
// Runs as an OBS tick callback on the graphics thread, like OBS's own
// ScreenshotObj (frontend/utility/ScreenshotObj.cpp):
//   tick N   : if a sample is due and the worker can accept it, render ONLY the
//              profile's regions of interest of the selected source into small
//              texrenders (GPU crop + downscale) and stage them.
//   tick N+1 : map the staging surfaces, copy the few KB of ROI pixels into a
//              pooled FramePacket and hand it to the analysis worker.
// Mapping one tick after staging avoids stalling on the GPU copy. Staging
// surfaces alternate between two buffers per region (double buffering).
// No detection work ever runs on the graphics thread.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "obs-raii.hpp"

#include "killsense/capture.hpp"
#include "killsense/profile.hpp"
#include "killsense/worker.hpp"

#include <obs.h>

#include <atomic>
#include <mutex>
#include <vector>

namespace killsense::obsplugin {

struct SamplerStats {
	bool running = false;
	bool sourceAvailable = false;
	uint32_t sourceWidth = 0;
	uint32_t sourceHeight = 0;
	uint64_t samplesDelivered = 0;
	uint64_t skippedWorkerBusy = 0;
	uint64_t mapFailures = 0;
	double avgTickUs = 0.0; // time spent inside the OBS graphics thread per sample
	double maxTickUs = 0.0;
	size_t regionCount = 0;
};

class FrameSampler {
public:
	FrameSampler(FramePool &pool, AnalysisWorker &worker);
	~FrameSampler();
	FrameSampler(const FrameSampler &) = delete;
	FrameSampler &operator=(const FrameSampler &) = delete;

	// Any thread. `source` may be nullptr; only a weak reference is kept.
	void setSource(obs_source_t *source);
	void setProfile(ProfilePtr profile, const PresetSettings &preset);

	// Any thread except the graphics thread.
	void start();
	void stop(); // removes the tick callback and frees GPU resources

	bool running() const { return running_.load(); }
	SamplerStats stats() const;

private:
	struct Slot {
		gs_texrender_t *texrender = nullptr;
		gs_stagesurf_t *stage[2] = {nullptr, nullptr};
		uint32_t width = 0;
		uint32_t height = 0;
		CapturePlanEntry entry;
	};

	static void tickCallback(void *param, float seconds);
	void tick();
	bool rebuildPlan(uint32_t width, uint32_t height); // graphics context required
	void renderAndStage(obs_source_t *source);         // graphics context required
	void downloadStaged();                             // graphics context required
	void releaseGraphics();                            // graphics context required

	FramePool &pool_;
	AnalysisWorker &worker_;
	std::atomic<bool> running_{false};

	// Shared with the UI/hotkey threads (guarded by mutex_).
	mutable std::mutex mutex_;
	WeakSourcePtr weakSource_;
	ProfilePtr profile_;
	PresetSettings preset_;
	bool configDirty_ = true;
	SamplerStats stats_;

	// Graphics-thread-only state.
	std::vector<Slot> slots_;
	CapturePlan plan_;
	ProfilePtr planProfile_;
	uint64_t intervalNs_ = 166666666;
	uint64_t nextSampleNs_ = 0;
	bool pending_ = false;
	int stageIndex_ = 0;
	int64_t pendingCaptureMs_ = 0;
	uint64_t sequence_ = 0;
};

} // namespace killsense::obsplugin
