// SPDX-License-Identifier: GPL-2.0-or-later
#include "killsense/session.hpp"

#include <cstdio>
#include <utility>

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

std::string kdRatioText(const SessionStats &s)
{
	char buf[32];
	const double kd = static_cast<double>(s.kills) / static_cast<double>(s.deaths > 0 ? s.deaths : 1);
	std::snprintf(buf, sizeof(buf), "%.2f", kd);
	return buf;
}

std::string formatCounter(const std::string &format, const SessionStats &s)
{
	const std::pair<const char *, std::string> vars[] = {{"{kills}", std::to_string(s.kills)},
							     {"{deaths}", std::to_string(s.deaths)},
							     {"{streak}", std::to_string(s.streak)},
							     {"{best}", std::to_string(s.bestStreak)},
							     {"{kd}", kdRatioText(s)}};
	std::string out = format;
	for (const auto &v : vars) {
		const std::string key = v.first;
		for (size_t pos = 0; (pos = out.find(key, pos)) != std::string::npos; pos += v.second.size())
			out.replace(pos, key.size(), v.second);
	}
	return out;
}

} // namespace killsense
