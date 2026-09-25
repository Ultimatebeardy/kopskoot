// SPDX-License-Identifier: GPL-2.0-or-later
#include "killsense/replay.hpp"

#include "killsense/fsutil.hpp"
#include "killsense/logging.hpp"
#include "killsense/ocr.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace killsense {

ReplayResult runReplay(const ReplayOptions &opt)
{
	ReplayResult result;
	auto parsed = loadProfileFile(opt.profileFile);
	if (!parsed.ok) {
		result.error = "profile invalid: " + (parsed.errors.empty() ? std::string("?") : parsed.errors.front());
		return result;
	}
	ProfilePtr profile = applyOverrides(*parsed.profile, opt.overrides);

	std::vector<std::filesystem::path> frames;
	std::error_code ec;
	for (std::filesystem::directory_iterator it(opt.framesDir, ec), end; !ec && it != end; it.increment(ec)) {
		const auto ext = it->path().extension().string();
		if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp")
			frames.push_back(it->path());
	}
	if (ec) {
		result.error = "cannot read frames directory: " + ec.message();
		return result;
	}
	std::sort(frames.begin(), frames.end());
	if (frames.empty()) {
		result.error = "no image frames found";
		return result;
	}

	std::string currentSidecar;
	std::unique_ptr<IOcrEngine> ocr;
	switch (opt.ocr) {
	case ReplayOcrMode::None:
		ocr = std::make_unique<NullOcrEngine>();
		break;
	case ReplayOcrMode::Sidecar:
		ocr = std::make_unique<ScriptedOcrEngine>([&currentSidecar](const Image &) { return currentSidecar; });
		break;
	case ReplayOcrMode::Platform:
		ocrThreadInit();
		ocr = createPlatformOcrEngine(opt.ocrLanguage);
		break;
	}

	const PresetSettings preset = presetSettings(opt.preset);
	DetectionEngine engine;
	engine.configure(profile, ocr.get(), preset);
	SessionTracker session;

	const double fps = opt.fps > 0.0 ? opt.fps : 6.0;
	Image full;
	FramePacket packet;
	FrameAnalysis analysis;
	CapturePlan plan;
	for (size_t i = 0; i < frames.size(); ++i) {
		if (!loadImageFile(pathToUtf8(frames[i]), full, PixelFormat::RGBA8)) {
			result.error = "cannot load frame " + pathToUtf8(frames[i]);
			engine.configure(nullptr, nullptr, preset);
			return result;
		}
		if (plan.sourceWidth != full.width() || plan.sourceHeight != full.height())
			plan = planCaptures(*profile, full.width(), full.height(), preset);

		currentSidecar.clear();
		std::filesystem::path sidecar = frames[i];
		sidecar.replace_extension(".ocr.txt");
		readTextFile(sidecar, currentSidecar, 64 * 1024);

		packet.sequence = i + 1;
		packet.captureTimeMs = static_cast<int64_t>(std::llround(i * 1000.0 / fps));
		buildPacketFromFullFrame(full, plan, packet);
		engine.process(packet, analysis);

		for (const auto &det : analysis.events) {
			KillEvent ev = makeKillEvent(det, profile.get(), "replay");
			ev.timestampMs = packet.captureTimeMs; // deterministic
			ev.timestamp = isoTimestampUtc(ev.timestampMs);
			session.record(ev);
			result.events.push_back(ev);
		}
		if (opt.collectFrameLog) {
			char buf[512];
			for (const auto &t : analysis.targets) {
				std::snprintf(
					buf, sizeof(buf),
					"%s t=%lldms proc=%.2fms target=%s conf=%.3f pass=%d active=%d emit=%d %s%s",
					pathToUtf8(frames[i].filename()).c_str(),
					static_cast<long long>(packet.captureTimeMs), analysis.processingMs,
					t.targetId.c_str(), t.fusion.confidence, t.fusion.passes ? 1 : 0,
					t.fusion.activeSignals, t.decision.emitCount, t.fusion.reason.c_str(),
					t.decision.suppressed
						? (" [suppressed: " + t.decision.suppressReason + "]").c_str()
						: "");
				result.frameLog.emplace_back(buf);
			}
		}
		++result.frames;
	}
	engine.configure(nullptr, nullptr, preset);
	if (opt.ocr == ReplayOcrMode::Platform) {
		ocr.reset();
		ocrThreadDeinit();
	}
	result.session = session.stats();
	result.ok = true;
	return result;
}

} // namespace killsense
