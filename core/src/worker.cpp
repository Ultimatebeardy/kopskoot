// SPDX-License-Identifier: GPL-2.0-or-later
#include "killsense/worker.hpp"

#include "killsense/logging.hpp"

#include <algorithm>
#include <chrono>

namespace killsense {

void BoundedFrameQueue::push(FramePacketPtr packet, FramePacketPtr &dropped)
{
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (closed_) {
			dropped = std::move(packet);
			return;
		}
		if (items_.size() >= capacity_) {
			dropped = std::move(items_.front());
			items_.pop_front();
		}
		items_.push_back(std::move(packet));
	}
	cv_.notify_one();
}

FramePacketPtr BoundedFrameQueue::pop(int timeoutMs)
{
	std::unique_lock<std::mutex> lock(mutex_);
	cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
		     [this] { return closed_ || wake_ || !items_.empty(); });
	wake_ = false;
	if (items_.empty())
		return nullptr;
	FramePacketPtr p = std::move(items_.front());
	items_.pop_front();
	return p;
}

void BoundedFrameQueue::close()
{
	{
		std::lock_guard<std::mutex> lock(mutex_);
		closed_ = true;
		items_.clear();
	}
	cv_.notify_all();
}

void BoundedFrameQueue::reopen()
{
	std::lock_guard<std::mutex> lock(mutex_);
	closed_ = false;
	wake_ = false;
}

void BoundedFrameQueue::wake()
{
	{
		std::lock_guard<std::mutex> lock(mutex_);
		wake_ = true;
	}
	cv_.notify_all();
}

size_t BoundedFrameQueue::size() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return items_.size();
}

AnalysisWorker::AnalysisWorker(FramePool &pool, OcrFactory ocrFactory, EventCallback onEvents, size_t queueCapacity)
	: pool_(pool),
	  ocrFactory_(std::move(ocrFactory)),
	  onEvents_(std::move(onEvents)),
	  queue_(queueCapacity)
{
}

AnalysisWorker::~AnalysisWorker()
{
	stop();
}

void AnalysisWorker::start()
{
	if (running_.load() || thread_.joinable())
		return;
	stopRequested_.store(false);
	queue_.reopen();
	{
		// The engine lives on the worker thread; re-apply the last profile.
		std::lock_guard<std::mutex> lock(configMutex_);
		if (pendingProfile_)
			configPending_ = true;
	}
	running_.store(true);
	{
		std::lock_guard<std::mutex> lock(statsMutex_);
		stats_.running = true;
	}
	thread_ = std::thread(&AnalysisWorker::threadMain, this);
}

void AnalysisWorker::stop()
{
	stopRequested_.store(true);
	queue_.close();
	if (thread_.joinable())
		thread_.join();
	running_.store(false);
	std::lock_guard<std::mutex> lock(statsMutex_);
	stats_.running = false;
	stats_.queueDepth = 0;
}

void AnalysisWorker::submit(FramePacketPtr packet)
{
	if (!packet)
		return;
	FramePacketPtr dropped;
	queue_.push(std::move(packet), dropped);
	{
		std::lock_guard<std::mutex> lock(statsMutex_);
		++stats_.framesSubmitted;
		if (dropped)
			++stats_.framesDropped;
	}
	if (dropped)
		pool_.release(std::move(dropped));
}

void AnalysisWorker::setProfile(ProfilePtr profile, const PresetSettings &preset)
{
	{
		std::lock_guard<std::mutex> lock(configMutex_);
		pendingProfile_ = std::move(profile);
		pendingPreset_ = preset;
		configPending_ = true;
	}
	queue_.wake();
}

void AnalysisWorker::requestTemporalReset()
{
	resetRequested_.store(true);
	queue_.wake();
}

bool AnalysisWorker::debugSnapshot(DebugSnapshot &out) const
{
	std::lock_guard<std::mutex> lock(debugMutex_);
	if (!debug_.valid)
		return false;
	out = debug_;
	return true;
}

WorkerStats AnalysisWorker::stats() const
{
	std::lock_guard<std::mutex> lock(statsMutex_);
	WorkerStats s = stats_;
	s.queueDepth = queue_.size();
	return s;
}

void AnalysisWorker::threadMain()
{
	ocrThreadInit();
	std::unique_ptr<IOcrEngine> ocr;
	try {
		ocr = ocrFactory_ ? ocrFactory_() : nullptr;
	} catch (const std::exception &e) {
		KS_LOG_WARN("OCR engine creation failed: %s", e.what());
	} catch (...) {
		KS_LOG_WARN("OCR engine creation failed (unknown error)");
	}
	{
		std::lock_guard<std::mutex> lock(statsMutex_);
		stats_.ocrEngine = ocr ? ocr->name() : "none";
		stats_.ocrAvailable = ocr && ocr->available();
	}

	DetectionEngine engine;
	FrameAnalysis analysis;
	KS_LOG_INFO("analysis worker started");

	while (!stopRequested_.load()) {
		{
			std::lock_guard<std::mutex> lock(configMutex_);
			if (configPending_) {
				configPending_ = false;
				try {
					engine.configure(pendingProfile_, ocr.get(), pendingPreset_);
				} catch (const std::exception &e) {
					KS_LOG_ERROR("engine configuration failed: %s", e.what());
					engine.configure(nullptr, nullptr, pendingPreset_);
				}
				std::lock_guard<std::mutex> slock(statsMutex_);
				stats_.unavailableDetectors = engine.unavailableDetectors();
				stats_.profileId = engine.profile() ? engine.profile()->id : std::string();
			}
		}
		if (resetRequested_.exchange(false))
			engine.resetTemporal();

		FramePacketPtr packet = queue_.pop(200);
		if (!packet)
			continue;

		bool ok = false;
		try {
			engine.process(*packet, analysis);
			ok = true;
		} catch (const std::exception &e) {
			KS_LOG_ERROR("frame analysis failed: %s", e.what());
		} catch (...) {
			KS_LOG_ERROR("frame analysis failed (unknown error)");
		}

		const int64_t now = monotonicMs();
		if (ok) {
			std::lock_guard<std::mutex> lock(statsMutex_);
			++stats_.framesProcessed;
			stats_.eventsDetected += analysis.events.size();
			stats_.lastProcessingMs = analysis.processingMs;
			const double a = 0.1; // EMA
			stats_.avgProcessingMs = stats_.framesProcessed == 1
							 ? analysis.processingMs
							 : stats_.avgProcessingMs * (1 - a) + analysis.processingMs * a;
			const double latency = static_cast<double>(now - packet->captureTimeMs);
			stats_.avgLatencyMs = stats_.framesProcessed == 1 ? latency
									  : stats_.avgLatencyMs * (1 - a) + latency * a;
			double best = 0.0;
			for (const auto &t : analysis.targets)
				best = std::max(best, t.fusion.confidence);
			stats_.lastConfidence = best;
			processedTimes_.push_back(now);
			while (!processedTimes_.empty() && now - processedTimes_.front() > 2000)
				processedTimes_.pop_front();
			stats_.analysisFps =
				processedTimes_.size() > 1
					? (processedTimes_.size() - 1) * 1000.0 /
						  std::max<int64_t>(1, processedTimes_.back() - processedTimes_.front())
					: 0.0;
		}

		if (ok && debugEnabled_.load()) {
			std::lock_guard<std::mutex> lock(debugMutex_);
			debug_.valid = true;
			debug_.analysis = analysis;
			debug_.regions = packet->regions; // copy only while debugging
		}

		if (ok && !analysis.events.empty() && onEvents_) {
			try {
				onEvents_(analysis.events, engine.profile());
			} catch (const std::exception &e) {
				KS_LOG_ERROR("event callback failed: %s", e.what());
			} catch (...) {
				KS_LOG_ERROR("event callback failed (unknown error)");
			}
		}
		pool_.release(std::move(packet));
	}

	engine.configure(nullptr, nullptr, PresetSettings{});
	ocr.reset();
	ocrThreadDeinit();
	KS_LOG_INFO("analysis worker stopped");
}

} // namespace killsense
