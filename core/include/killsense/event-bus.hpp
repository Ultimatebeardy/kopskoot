// REALM KillSense - internal event bus.
// publish() never blocks the caller: events are queued (bounded) and delivered
// to subscribers on a dedicated dispatch thread, so a slow output (network,
// disk) can never stall detection or OBS.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "killsense/event.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace killsense {

// Output sink interface (obs-websocket vendor, JSONL file, future Streamer.bot
// or localhost server outputs all implement this).
class IEventOutput {
public:
	virtual ~IEventOutput() = default;
	virtual const char *name() const = 0;
	virtual bool enabled() const = 0;
	virtual void deliver(const KillEvent &ev) = 0; // called on the dispatch thread
};

class EventBus {
public:
	using Callback = std::function<void(const KillEvent &)>;
	using SubscriptionId = uint64_t;

	explicit EventBus(size_t capacity = 128);
	~EventBus();
	EventBus(const EventBus &) = delete;
	EventBus &operator=(const EventBus &) = delete;

	void start();
	void stop(); // delivers nothing further; joins the thread

	SubscriptionId subscribe(Callback cb);
	void unsubscribe(SubscriptionId id);
	void addOutput(std::shared_ptr<IEventOutput> output);
	void removeOutput(const std::shared_ptr<IEventOutput> &output);

	// Returns false if the queue was full and the event was dropped.
	bool publish(const KillEvent &ev);

	// Synchronously drains the queue (tests / replay tool).
	void flush();
	uint64_t dropped() const { return dropped_.load(); }

private:
	void threadMain();
	void deliver(const KillEvent &ev);

	size_t capacity_;
	std::mutex mutex_;
	std::condition_variable cv_;
	std::condition_variable idleCv_;
	std::deque<KillEvent> queue_;
	bool stop_ = false;
	bool busy_ = false;
	std::thread thread_;
	std::atomic<uint64_t> dropped_{0};

	std::mutex subsMutex_;
	std::vector<std::pair<SubscriptionId, Callback>> subscribers_;
	std::vector<std::shared_ptr<IEventOutput>> outputs_;
	SubscriptionId nextId_ = 1;
};

// Appends one JSON line per event. Disabled by default; path chosen by the user.
class JsonlFileOutput final : public IEventOutput {
public:
	explicit JsonlFileOutput(std::string pathUtf8);
	const char *name() const override { return "jsonl-file"; }
	bool enabled() const override { return enabled_.load(); }
	void setEnabled(bool e) { enabled_.store(e); }
	void setPath(const std::string &pathUtf8);
	void deliver(const KillEvent &ev) override;

private:
	std::mutex mutex_;
	std::string path_;
	std::atomic<bool> enabled_{false};
};

} // namespace killsense
