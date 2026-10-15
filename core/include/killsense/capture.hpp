// KopSkoot - capture planning and frame packets.
// The OBS frame sampler and the replay tool both use planCaptures() so the
// detector sees identical region images in OBS and in offline tests.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "killsense/geometry.hpp"
#include "killsense/image.hpp"
#include "killsense/profile.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace killsense {

enum class PerformancePreset { LowImpact, Balanced, HighAccuracy };

struct PresetSettings {
	double analysisFps = 6.0;     // frames per second handed to the detector
	double captureScale = 1.0;    // region pixels per reference (1080p) pixel
	int ocrMinIntervalMs = 150;   // minimum time between OCR runs per detector
	int maxRegionPixels = 600000; // hard cap per captured region
};

PresetSettings presetSettings(PerformancePreset preset);
const char *presetId(PerformancePreset preset);
bool parsePreset(const std::string &id, PerformancePreset &out);

struct CapturePlanEntry {
	std::string regionId;
	PixelRect sourceRect; // region in source pixels
	int outWidth = 0;     // captured (staged) size
	int outHeight = 0;
	double pxPerReference = 1.0; // captured pixels per reference-resolution pixel
};

struct CapturePlan {
	int sourceWidth = 0;
	int sourceHeight = 0;
	std::vector<CapturePlanEntry> entries;
	bool empty() const { return entries.empty(); }
};

// Computes which regions to capture and at which size. Only regions used by at
// least one enabled detector are captured. Never upscales on the GPU.
CapturePlan planCaptures(const GameProfile &profile, int sourceWidth, int sourceHeight, const PresetSettings &preset);

struct RegionCapture {
	std::string regionId;
	PixelRect sourceRect;
	double pxPerReference = 1.0;
	Image rgba;
};

// One sampled analysis frame: only the captured regions, never the full frame.
struct FramePacket {
	uint64_t sequence = 0;
	int64_t captureTimeMs = 0; // monotonic milliseconds
	int sourceWidth = 0;
	int sourceHeight = 0;
	std::vector<RegionCapture> regions;

	const RegionCapture *findRegion(const std::string &id) const;
};

using FramePacketPtr = std::unique_ptr<FramePacket>;

// Recycles FramePackets (and their pixel buffers) between the sampler and the
// worker so steady-state operation performs no allocations.
class FramePool {
public:
	explicit FramePool(size_t maxPooled = 4) : maxPooled_(maxPooled) {}
	FramePacketPtr acquire();
	void release(FramePacketPtr packet);

private:
	std::mutex mutex_;
	std::vector<FramePacketPtr> free_;
	size_t maxPooled_;
};

// Builds a packet from a full frame (replay / tests), emulating the GPU path:
// crop each planned region and scale it to the planned output size.
void buildPacketFromFullFrame(const Image &fullRgba, const CapturePlan &plan, FramePacket &out);

int64_t monotonicMs();

} // namespace killsense
