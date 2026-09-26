// REALM KillSense - session counters (kills, streak, deaths). Thread-safe.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "killsense/event.hpp"

#include <mutex>
#include <optional>
#include <string>

namespace killsense {

struct SessionStats {
	int64_t kills = 0;
	int64_t streak = 0;
	int64_t bestStreak = 0;
	int64_t deaths = 0;
	int64_t events = 0;
	int64_t sequence = 0;
	std::optional<KillEvent> lastEvent;
};

// K/D ratio as shown to viewers: kills / max(deaths, 1), two decimals.
std::string kdRatioText(const SessionStats &s);
// Replaces {kills} {deaths} {streak} {best} {kd} in `format`.
std::string formatCounter(const std::string &format, const SessionStats &s);

class SessionTracker {
public:
	// Updates counters and fills sessionKills/killStreak/... fields of `ev`.
	void record(KillEvent &ev);
	// Resets counters; returns a SessionReset event describing the reset.
	KillEvent reset(const std::string &profileId, const std::string &game);
	SessionStats stats() const;

private:
	mutable std::mutex mutex_;
	SessionStats stats_;
};

} // namespace killsense
