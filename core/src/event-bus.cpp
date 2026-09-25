// SPDX-License-Identifier: GPL-2.0-or-later
#include "killsense/event-bus.hpp"

#include "killsense/fsutil.hpp"
#include "killsense/logging.hpp"

#include <algorithm>
#include <fstream>

namespace killsense {

EventBus::EventBus(size_t capacity) : capacity_(capacity ? capacity : 1) {}

EventBus::~EventBus()
{
	stop();
}

void EventBus::start()
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (thread_.joinable())
		return;
	stop_ = false;
	thread_ = std::thread(&EventBus::threadMain, this);
}

void EventBus::stop()
{
	{
		std::lock_guard<std::mutex> lock(mutex_);
		stop_ = true;
	}
	cv_.notify_all();
	if (thread_.joinable())
		thread_.join();
	std::lock_guard<std::mutex> lock(mutex_);
	queue_.clear();
	idleCv_.notify_all();
}

EventBus::SubscriptionId EventBus::subscribe(Callback cb)
{
	std::lock_guard<std::mutex> lock(subsMutex_);
	const SubscriptionId id = nextId_++;
	subscribers_.emplace_back(id, std::move(cb));
	return id;
}

void EventBus::unsubscribe(SubscriptionId id)
{
	std::lock_guard<std::mutex> lock(subsMutex_);
	subscribers_.erase(std::remove_if(subscribers_.begin(), subscribers_.end(),
					  [id](const auto &p) { return p.first == id; }),
			   subscribers_.end());
}

void EventBus::addOutput(std::shared_ptr<IEventOutput> output)
{
	if (!output)
		return;
	std::lock_guard<std::mutex> lock(subsMutex_);
	outputs_.push_back(std::move(output));
}

void EventBus::removeOutput(const std::shared_ptr<IEventOutput> &output)
{
	std::lock_guard<std::mutex> lock(subsMutex_);
	outputs_.erase(std::remove(outputs_.begin(), outputs_.end(), output), outputs_.end());
}

bool EventBus::publish(const KillEvent &ev)
{
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (stop_ || !thread_.joinable()) {
			++dropped_;
			return false;
		}
		if (queue_.size() >= capacity_) {
			++dropped_;
			KS_LOG_WARN("event bus queue full; dropping %s event", eventTypeId(ev.type));
			return false;
		}
		queue_.push_back(ev);
	}
	cv_.notify_one();
	return true;
}

void EventBus::flush()
{
	std::unique_lock<std::mutex> lock(mutex_);
	idleCv_.wait_for(lock, std::chrono::seconds(5), [this] { return stop_ || (queue_.empty() && !busy_); });
}

void EventBus::deliver(const KillEvent &ev)
{
	std::vector<Callback> callbacks;
	std::vector<std::shared_ptr<IEventOutput>> outputs;
	{
		std::lock_guard<std::mutex> lock(subsMutex_);
		for (const auto &p : subscribers_)
			callbacks.push_back(p.second);
		outputs = outputs_;
	}
	for (auto &cb : callbacks) {
		try {
			cb(ev);
		} catch (const std::exception &e) {
			KS_LOG_ERROR("event subscriber threw: %s", e.what());
		} catch (...) {
			KS_LOG_ERROR("event subscriber threw (unknown)");
		}
	}
	for (auto &o : outputs) {
		try {
			if (o->enabled())
				o->deliver(ev);
		} catch (const std::exception &e) {
			KS_LOG_ERROR("event output '%s' threw: %s", o->name(), e.what());
		} catch (...) {
			KS_LOG_ERROR("event output '%s' threw (unknown)", o->name());
		}
	}
}

void EventBus::threadMain()
{
	for (;;) {
		KillEvent ev;
		{
			std::unique_lock<std::mutex> lock(mutex_);
			busy_ = false;
			idleCv_.notify_all();
			cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });
			if (stop_)
				return;
			ev = std::move(queue_.front());
			queue_.pop_front();
			busy_ = true;
		}
		deliver(ev);
	}
}

JsonlFileOutput::JsonlFileOutput(std::string pathUtf8) : path_(std::move(pathUtf8)) {}

void JsonlFileOutput::setPath(const std::string &pathUtf8)
{
	std::lock_guard<std::mutex> lock(mutex_);
	path_ = pathUtf8;
}

void JsonlFileOutput::deliver(const KillEvent &ev)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (path_.empty())
		return;
	std::ofstream out(pathFromUtf8(path_), std::ios::app | std::ios::binary);
	if (!out) {
		KS_LOG_WARN("JSONL output: cannot open '%s'", path_.c_str());
		return;
	}
	out << toJson(ev, false) << '\n';
}

} // namespace killsense
