// SPDX-License-Identifier: GPL-2.0-or-later
#include "killsense/session.hpp"

#include <algorithm>

namespace killsense {

void SessionTracker::record(KillEvent &ev)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (countsAsKill(ev.type)) {
		++stats_.kills;
		++stats_.streak;
		stats_.bestStreak = std::max(stats_.bestStreak, stats_.streak);
	} else if (ev.type == EventType::Death) {
		++stats_.deaths;
		stats_.streak = 0;
	}
	++stats_.events;
	ev.sequence = ++stats_.sequence;
	ev.sessionKills = stats_.kills;
	ev.killStreak = stats_.streak;
	ev.bestStreak = stats_.bestStreak;
	ev.sessionDeaths = stats_.deaths;
	if (ev.timestampMs == 0)
		ev.timestampMs = nowUnixMs();
	if (ev.timestamp.empty())
		ev.timestamp = isoTimestampUtc(ev.timestampMs);
	stats_.lastEvent = ev;
}

KillEvent SessionTracker::reset(const std::string &profileId, const std::string &game)
{
	std::lock_guard<std::mutex> lock(mutex_);
	stats_ = SessionStats{};
	KillEvent ev;
	ev.type = EventType::SessionReset;
	ev.profile = profileId;
	ev.game = game;
	ev.confidence = 1.0;
	ev.detector = "manual";
	ev.timestampMs = nowUnixMs();
	ev.timestamp = isoTimestampUtc(ev.timestampMs);
	ev.sequence = 0;
	return ev;
}

SessionStats SessionTracker::stats() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return stats_;
}

} // namespace killsense
