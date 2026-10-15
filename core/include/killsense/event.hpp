// KopSkoot - common event schema (see docs/EVENT_SCHEMA.md).
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace killsense {

enum class EventType {
	Kill,
	Headshot,
	DoubleKill,
	TripleKill,
	MultiKill,
	Assist,
	EnemyDown,
	Death,
	VehicleDestroyed,
	BossKill,
	SessionReset,
};

// Lower-case schema id: "kill", "headshot", "double_kill", ...
const char *eventTypeId(EventType type);
// obs-websocket vendor event name: "Kill", "Headshot", "DoubleKill", ...
const char *eventTypeVendorName(EventType type);
std::optional<EventType> parseEventType(std::string_view id);

// Whether the event increments the session kill counter / streak.
bool countsAsKill(EventType type);
// Whether the event ends the current kill streak.
bool endsStreak(EventType type);

struct EvidenceItem {
	std::string detector; // detector id from the profile
	std::string kind;     // template / ocr / color / hud_change / ...
	double weight = 0.0;
	double score = 0.0;
	double contribution = 0.0;
	bool active = false;
};

struct KillEvent {
	int schemaVersion = 1;
	EventType type = EventType::Kill;
	std::string game;    // profile display name (or "unknown")
	std::string profile; // profile id
	std::string target;  // profile target id that produced the event
	double confidence = 0.0;
	int64_t sessionKills = 0;
	int64_t killStreak = 0;
	int64_t bestStreak = 0;
	int64_t sessionDeaths = 0;
	int64_t sequence = 0;    // monotonically increasing per session
	std::string timestamp;   // ISO-8601 UTC with milliseconds
	int64_t timestampMs = 0; // unix epoch milliseconds
	std::string detector;    // "vision", "test", "manual"
	bool test = false;       // true for Test Kill Event
	std::string reason;      // short human readable decision reason
	std::vector<EvidenceItem> evidence;
};

// Serializes to the documented JSON schema. `includeEvidence` adds the
// "evidence" array (useful for debugging, off for vendor events by default).
std::string toJson(const KillEvent &ev, bool includeEvidence = true, int indent = -1);
bool fromJson(const std::string &json, KillEvent &out, std::string *error = nullptr);

std::string isoTimestampUtc(int64_t unixMs);
int64_t nowUnixMs();

} // namespace killsense
