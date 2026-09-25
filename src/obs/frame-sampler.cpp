// SPDX-License-Identifier: GPL-2.0-or-later
#include "frame-sampler.hpp"

#include "killsense/logging.hpp"

#include <graphics/graphics.h>
#include <graphics/vec4.h>
#include <util/platform.h>

#include <algorithm>
#include <cmath>

namespace killsense::obsplugin {

FrameSampler::FrameSampler(FramePool &pool, AnalysisWorker &worker) : pool_(pool), worker_(worker) {}

FrameSampler::~FrameSampler()
{
	stop();
}

void FrameSampler::setSource(obs_source_t *source)
{
	WeakSourcePtr weak(source ? obs_source_get_weak_source(source) : nullptr);
	std::lock_guard<std::mutex> lock(mutex_);
	weakSource_ = std::move(weak);
	configDirty_ = true;
}

void FrameSampler::setProfile(ProfilePtr profile, const PresetSettings &preset)
{
	std::lock_guard<std::mutex> lock(mutex_);
	profile_ = std::move(profile);
	preset_ = preset;
	configDirty_ = true;
}

void FrameSampler::start()
{
	if (running_.exchange(true))
		return;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		configDirty_ = true;
		stats_.running = true;
	}
	nextSampleNs_ = 0;
	pending_ = false;
	obs_add_tick_callback(&FrameSampler::tickCallback, this);
	KS_LOG_INFO("frame sampler started");
}

void FrameSampler::stop()
{
	const bool wasRunning = running_.exchange(false);
	if (wasRunning) {
		// Tick callbacks run while libobs holds draw_callbacks_mutex; removal
		// takes the same mutex, so after this returns no tick is in flight.
		obs_remove_tick_callback(&FrameSampler::tickCallback, this);
	}
	if (!slots_.empty()) {
		obs_enter_graphics();
		releaseGraphics();
		obs_leave_graphics();
	}
	pending_ = false;
	plan_ = CapturePlan{};
	planProfile_.reset();
	std::lock_guard<std::mutex> lock(mutex_);
	stats_.running = false;
	if (wasRunning)
		KS_LOG_INFO("frame sampler stopped");
}

SamplerStats FrameSampler::stats() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return stats_;
}

void FrameSampler::tickCallback(void *param, float)
{
	static_cast<FrameSampler *>(param)->tick();
}

void FrameSampler::tick()
{
	if (!running_.load())
		return;
	const uint64_t now = os_gettime_ns();
	if (!pending_ && now < nextSampleNs_)
		return; // cheap early-out on the vast majority of frames

	SourcePtr source;
	ProfilePtr profile;
	PresetSettings preset;
	bool dirty = false;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (weakSource_)
			source.reset(obs_weak_source_get_source(weakSource_.get()));
		profile = profile_;
		preset = preset_;
		dirty = configDirty_;
		configDirty_ = false;
	}

	bool didWork = false;
	bool checkedSource = false;
	bool sourceOk = false;
	uint32_t width = 0, height = 0;
	uint64_t skipped = 0;

	obs_enter_graphics();
	if (pending_) {
		downloadStaged();
		pending_ = false;
		didWork = true;
	} else if (source && profile) {
		checkedSource = true;
		width = obs_source_get_width(source.get());
		height = obs_source_get_height(source.get());
		sourceOk = width > 0 && height > 0;
		if (sourceOk) {
			if (dirty || profile != planProfile_ || width != static_cast<uint32_t>(plan_.sourceWidth) ||
			    height != static_cast<uint32_t>(plan_.sourceHeight)) {
				planProfile_ = profile;
				intervalNs_ = static_cast<uint64_t>(1e9 / std::clamp(preset.analysisFps, 0.5, 30.0));
				rebuildPlan(width, height);
			}
			if (slots_.empty()) {
				// Nothing to capture for this profile.
			} else if (!worker_.canAccept()) {
				skipped = 1; // worker overloaded: skip instead of queueing work
			} else {
				renderAndStage(source.get());
				pending_ = true;
				pendingCaptureMs_ = monotonicMs();
				didWork = true;
			}
		}
		nextSampleNs_ = now + intervalNs_;
	} else {
		checkedSource = true;
		nextSampleNs_ = now + intervalNs_;
	}
	obs_leave_graphics();

	const double us = static_cast<double>(os_gettime_ns() - now) / 1000.0;
	std::lock_guard<std::mutex> lock(mutex_);
	if (checkedSource) {
		stats_.sourceAvailable = sourceOk;
		stats_.sourceWidth = width;
		stats_.sourceHeight = height;
	}
	stats_.skippedWorkerBusy += skipped;
	stats_.regionCount = slots_.size();
	if (didWork) {
		stats_.avgTickUs = stats_.avgTickUs == 0.0 ? us : stats_.avgTickUs * 0.9 + us * 0.1;
		stats_.maxTickUs = std::max(stats_.maxTickUs * 0.999, us);
	}
}

bool FrameSampler::rebuildPlan(uint32_t width, uint32_t height)
{
	releaseGraphics();
	pending_ = false;
	PresetSettings preset;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		preset = preset_;
	}
	plan_ = planCaptures(*planProfile_, static_cast<int>(width), static_cast<int>(height), preset);
	plan_.sourceWidth = static_cast<int>(width);
	plan_.sourceHeight = static_cast<int>(height);
	for (const auto &e : plan_.entries) {
		Slot slot;
		slot.entry = e;
		slot.width = static_cast<uint32_t>(e.outWidth);
		slot.height = static_cast<uint32_t>(e.outHeight);
		slot.texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
		slot.stage[0] = gs_stagesurface_create(slot.width, slot.height, GS_RGBA);
		slot.stage[1] = gs_stagesurface_create(slot.width, slot.height, GS_RGBA);
		if (!slot.texrender || !slot.stage[0] || !slot.stage[1]) {
			KS_LOG_ERROR("failed to allocate capture surfaces for region '%s' (%ux%u)", e.regionId.c_str(),
				     slot.width, slot.height);
			gs_texrender_destroy(slot.texrender);
			gs_stagesurface_destroy(slot.stage[0]);
			gs_stagesurface_destroy(slot.stage[1]);
			continue;
		}
		slots_.push_back(slot);
	}
	KS_LOG_INFO("capture plan: source %ux%u, %zu region(s)", width, height, slots_.size());
	for (const auto &s : slots_)
		KS_LOG_DEBUG("  region '%s' src=(%d,%d %dx%d) -> %ux%u", s.entry.regionId.c_str(), s.entry.sourceRect.x,
			     s.entry.sourceRect.y, s.entry.sourceRect.w, s.entry.sourceRect.h, s.width, s.height);
	return !slots_.empty();
}

void FrameSampler::renderAndStage(obs_source_t *source)
{
	struct vec4 clearColor;
	vec4_zero(&clearColor);
	for (auto &slot : slots_) {
		const PixelRect &r = slot.entry.sourceRect;
		gs_texrender_reset(slot.texrender);
		// GS_CS_SRGB tonemaps HDR sources to SDR, as OBS's screenshot code does.
		if (!gs_texrender_begin_with_color_space(slot.texrender, slot.width, slot.height, GS_CS_SRGB))
			continue;
		gs_clear(GS_CLEAR_COLOR, &clearColor, 0.0f, 0);
		// Orthographic projection over the ROI only: the GPU crops and scales,
		// so only the region's pixels are ever transferred to the CPU.
		gs_ortho(static_cast<float>(r.x), static_cast<float>(r.x + r.w), static_cast<float>(r.y),
			 static_cast<float>(r.y + r.h), -100.0f, 100.0f);
		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
		obs_source_video_render(source);
		gs_blend_state_pop();
		gs_texrender_end(slot.texrender);
		gs_stage_texture(slot.stage[stageIndex_], gs_texrender_get_texture(slot.texrender));
	}
}

void FrameSampler::downloadStaged()
{
	const int idx = stageIndex_;
	stageIndex_ ^= 1; // next render stages into the other buffer
	FramePacketPtr packet = pool_.acquire();
	packet->regions.resize(slots_.size());
	for (size_t i = 0; i < slots_.size(); ++i) {
		Slot &slot = slots_[i];
		uint8_t *data = nullptr;
		uint32_t linesize = 0;
		if (!gs_stagesurface_map(slot.stage[idx], &data, &linesize)) {
			std::lock_guard<std::mutex> lock(mutex_);
			++stats_.mapFailures;
			pool_.release(std::move(packet));
			return;
		}
		RegionCapture &rc = packet->regions[i];
		rc.regionId = slot.entry.regionId;
		rc.sourceRect = slot.entry.sourceRect;
		rc.pxPerReference = slot.entry.pxPerReference;
		copyRgbaFromStrided(data, linesize, static_cast<int>(slot.width), static_cast<int>(slot.height),
				    rc.rgba);
		gs_stagesurface_unmap(slot.stage[idx]);
	}
	packet->sequence = ++sequence_;
	packet->captureTimeMs = pendingCaptureMs_;
	packet->sourceWidth = plan_.sourceWidth;
	packet->sourceHeight = plan_.sourceHeight;
	worker_.submit(std::move(packet));
	std::lock_guard<std::mutex> lock(mutex_);
	++stats_.samplesDelivered;
}

void FrameSampler::releaseGraphics()
{
	for (auto &slot : slots_) {
		gs_texrender_destroy(slot.texrender);
		gs_stagesurface_destroy(slot.stage[0]);
		gs_stagesurface_destroy(slot.stage[1]);
	}
	slots_.clear();
}

} // namespace killsense::obsplugin
