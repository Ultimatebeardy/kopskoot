// REALM KillSense - analysis worker thread and bounded frame queue.
//
// Thread ownership:
//   * The OBS graphics thread (FrameSampler) produces FramePackets.
//   * AnalysisWorker owns ONE std::thread that owns the DetectionEngine and the
//     OCR engine; nothing else touches them.
//   * Queue capacity is small (default 2). When full, the OLDEST packet is
//     dropped and counted, so OBS is never blocked and results stay fresh.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "killsense/capture.hpp"
#include "killsense/engine.hpp"
#include "killsense/ocr.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace killsense {

class BoundedFrameQueue {
public:
	explicit BoundedFrameQueue(size_t capacity) : capacity_(capacity ? capacity : 1) {}
	// Pushes a packet. If full, the oldest packet is removed and returned in
	// `dropped` (so the caller can recycle it). Never blocks.
	void push(FramePacketPtr packet, FramePacketPtr &dropped);
	// Waits up to `timeoutMs` for a packet. Returns nullptr on timeout/close.
	FramePacketPtr pop(int timeoutMs);
	void close();
	void reopen();
	size_t size() const;
	size_t capacity() const { return capacity_; }
	void wake();

private:
	mutable std::mutex mutex_;
	std::condition_variable cv_;
	std::deque<FramePacketPtr> items_;
	size_t capacity_;
	bool closed_ = false;
	bool wake_ = false;
};

struct WorkerStats {
	bool running = false;
	uint64_t framesSubmitted = 0;
	uint64_t framesProcessed = 0;
	uint64_t framesDropped = 0;
	uint64_t eventsDetected = 0;
	double analysisFps = 0.0;
	double lastProcessingMs = 0.0;
	double avgProcessingMs = 0.0;
	double avgLatencyMs = 0.0; // capture -> analysis finished
	size_t queueDepth = 0;
	std::string ocrEngine = "none";
	bool ocrAvailable = false;
	std::vector<std::string> unavailableDetectors;
	std::string profileId;
	double lastConfidence = 0.0;
};

struct DebugSnapshot {
	bool valid = false;
	FrameAnalysis analysis;
	std::vector<RegionCapture> regions; // copies of the analysed region images
};

class AnalysisWorker {
public:
	using EventCallback = std::function<void(const std::vector<DetectedEvent> &events, const ProfilePtr &profile)>;
	using OcrFactory = std::function<std::unique_ptr<IOcrEngine>()>;

	AnalysisWorker(FramePool &pool, OcrFactory ocrFactory, EventCallback onEvents, size_t queueCapacity = 2);
	~AnalysisWorker();
	AnalysisWorker(const AnalysisWorker &) = delete;
	AnalysisWorker &operator=(const AnalysisWorker &) = delete;

	void start();
	void stop(); // joins the thread; safe to call repeatedly

	bool running() const { return running_.load(); }
	bool canAccept() const { return running_.load() && queue_.size() < queue_.capacity(); }
	void submit(FramePacketPtr packet);

	// Applied on the worker thread before the next frame.
	void setProfile(ProfilePtr profile, const PresetSettings &preset);
	void requestTemporalReset();

	void setDebugEnabled(bool enabled) { debugEnabled_.store(enabled); }
	bool debugEnabled() const { return debugEnabled_.load(); }
	bool debugSnapshot(DebugSnapshot &out) const;

	WorkerStats stats() const;

private:
	void threadMain();

	FramePool &pool_;
	OcrFactory ocrFactory_;
	EventCallback onEvents_;
	BoundedFrameQueue queue_;
	std::thread thread_;
	std::atomic<bool> running_{false};
	std::atomic<bool> stopRequested_{false};
	std::atomic<bool> debugEnabled_{false};
	std::atomic<bool> resetRequested_{false};

	std::mutex configMutex_;
	bool configPending_ = false;
	ProfilePtr pendingProfile_;
	PresetSettings pendingPreset_;

	mutable std::mutex statsMutex_;
	WorkerStats stats_;
	std::deque<int64_t> processedTimes_;

	mutable std::mutex debugMutex_;
	DebugSnapshot debug_;
};

} // namespace killsense
