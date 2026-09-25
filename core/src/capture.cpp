// SPDX-License-Identifier: GPL-2.0-or-later
#include "killsense/capture.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <set>

namespace killsense {

PresetSettings presetSettings(PerformancePreset preset)
{
	PresetSettings s;
	switch (preset) {
	case PerformancePreset::LowImpact:
		s.analysisFps = 3.0;
		s.captureScale = 0.75;
		s.ocrMinIntervalMs = 400;
		s.maxRegionPixels = 250000;
		break;
	case PerformancePreset::Balanced:
		s.analysisFps = 6.0;
		s.captureScale = 1.0;
		s.ocrMinIntervalMs = 150;
		s.maxRegionPixels = 600000;
		break;
	case PerformancePreset::HighAccuracy:
		s.analysisFps = 10.0;
		s.captureScale = 1.33;
		s.ocrMinIntervalMs = 0;
		s.maxRegionPixels = 1200000;
		break;
	}
	return s;
}

const char *presetId(PerformancePreset preset)
{
	switch (preset) {
	case PerformancePreset::LowImpact:
		return "low_impact";
	case PerformancePreset::Balanced:
		return "balanced";
	case PerformancePreset::HighAccuracy:
		return "high_accuracy";
	}
	return "balanced";
}

bool parsePreset(const std::string &id, PerformancePreset &out)
{
	if (id == "low_impact")
		out = PerformancePreset::LowImpact;
	else if (id == "balanced")
		out = PerformancePreset::Balanced;
	else if (id == "high_accuracy")
		out = PerformancePreset::HighAccuracy;
	else
		return false;
	return true;
}

CapturePlan planCaptures(const GameProfile &profile, int sourceWidth, int sourceHeight, const PresetSettings &preset)
{
	CapturePlan plan;
	plan.sourceWidth = sourceWidth;
	plan.sourceHeight = sourceHeight;
	if (sourceWidth <= 0 || sourceHeight <= 0)
		return plan;

	std::set<std::string> used;
	for (const auto &d : profile.detectors)
		if (d.enabled)
			used.insert(d.region);

	const auto rects = profile.resolveRegions(static_cast<double>(sourceWidth) / sourceHeight);
	const double refH = profile.referenceHeight > 0 ? profile.referenceHeight : 1080.0;
	// Source pixels per reference pixel (e.g. 1440p source, 1080p ref => 1.333).
	const double srcPerRef = static_cast<double>(sourceHeight) / refH;

	for (const auto &region : profile.regions) {
		if (!used.count(region.id))
			continue;
		const auto it = rects.find(region.id);
		if (it == rects.end())
			continue;
		CapturePlanEntry e;
		e.regionId = region.id;
		e.sourceRect = toPixelRect(it->second, sourceWidth, sourceHeight);
		if (e.sourceRect.empty())
			continue;
		// Desired output scale relative to the source; never upscale on GPU.
		double scale = std::min(1.0, preset.captureScale / srcPerRef);
		const double pixels = static_cast<double>(e.sourceRect.w) * e.sourceRect.h * scale * scale;
		if (pixels > preset.maxRegionPixels)
			scale *= std::sqrt(preset.maxRegionPixels / pixels);
		e.outWidth = std::max(1, static_cast<int>(std::lround(e.sourceRect.w * scale)));
		e.outHeight = std::max(1, static_cast<int>(std::lround(e.sourceRect.h * scale)));
		e.pxPerReference = srcPerRef * (static_cast<double>(e.outHeight) / e.sourceRect.h);
		plan.entries.push_back(std::move(e));
	}
	return plan;
}

const RegionCapture *FramePacket::findRegion(const std::string &id) const
{
	for (const auto &r : regions)
		if (r.regionId == id)
			return &r;
	return nullptr;
}

FramePacketPtr FramePool::acquire()
{
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (!free_.empty()) {
			FramePacketPtr p = std::move(free_.back());
			free_.pop_back();
			return p;
		}
	}
	return std::make_unique<FramePacket>();
}

void FramePool::release(FramePacketPtr packet)
{
	if (!packet)
		return;
	std::lock_guard<std::mutex> lock(mutex_);
	if (free_.size() < maxPooled_)
		free_.push_back(std::move(packet));
}

void buildPacketFromFullFrame(const Image &fullRgba, const CapturePlan &plan, FramePacket &out)
{
	out.sourceWidth = fullRgba.width();
	out.sourceHeight = fullRgba.height();
	out.regions.resize(plan.entries.size());
	Image cropped;
	for (size_t i = 0; i < plan.entries.size(); ++i) {
		const auto &e = plan.entries[i];
		auto &rc = out.regions[i];
		rc.regionId = e.regionId;
		rc.sourceRect = e.sourceRect;
		rc.pxPerReference = e.pxPerReference;
		crop(fullRgba, e.sourceRect, cropped);
		resize(cropped, e.outWidth, e.outHeight, rc.rgba);
	}
}

int64_t monotonicMs()
{
	using namespace std::chrono;
	return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

} // namespace killsense
