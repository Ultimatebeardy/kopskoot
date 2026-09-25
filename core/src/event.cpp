// SPDX-License-Identifier: GPL-2.0-or-later
#include "killsense/event.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdio>
#include <ctime>

namespace killsense {

namespace {
struct TypeInfo {
	EventType type;
	const char *id;
	const char *vendor;
};

constexpr TypeInfo kTypes[] = {
	{EventType::Kill, "kill", "Kill"},
	{EventType::Headshot, "headshot", "Headshot"},
	{EventType::DoubleKill, "double_kill", "DoubleKill"},
	{EventType::TripleKill, "triple_kill", "TripleKill"},
	{EventType::MultiKill, "multi_kill", "MultiKill"},
	{EventType::Assist, "assist", "Assist"},
	{EventType::EnemyDown, "enemy_down", "EnemyDown"},
	{EventType::Death, "death", "Death"},
	{EventType::VehicleDestroyed, "vehicle_destroyed", "VehicleDestroyed"},
	{EventType::BossKill, "boss_kill", "BossKill"},
	{EventType::SessionReset, "session_reset", "SessionReset"},
};
} // namespace

const char *eventTypeId(EventType type)
{
	for (const auto &t : kTypes)
		if (t.type == type)
			return t.id;
	return "kill";
}

const char *eventTypeVendorName(EventType type)
{
	for (const auto &t : kTypes)
		if (t.type == type)
			return t.vendor;
	return "Kill";
}

std::optional<EventType> parseEventType(std::string_view id)
{
	for (const auto &t : kTypes)
		if (id == t.id || id == t.vendor)
			return t.type;
	return std::nullopt;
}

bool countsAsKill(EventType type)
{
	switch (type) {
	case EventType::Kill:
	case EventType::Headshot:
	case EventType::BossKill:
		return true;
	// Double/Triple/Multi kill are *announcements* that accompany individual
	// kill events; they must not be counted twice.
	default:
		return false;
	}
}

bool endsStreak(EventType type)
{
	return type == EventType::Death || type == EventType::SessionReset;
}

int64_t nowUnixMs()
{
	using namespace std::chrono;
	return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string isoTimestampUtc(int64_t unixMs)
{
	const std::time_t secs = static_cast<std::time_t>(unixMs / 1000);
	const int ms = static_cast<int>(((unixMs % 1000) + 1000) % 1000);
	std::tm tmv{};
#if defined(_WIN32)
	gmtime_s(&tmv, &secs);
#else
	gmtime_r(&secs, &tmv);
#endif
	char buf[64];
	std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", tmv.tm_year + 1900, tmv.tm_mon + 1,
		      tmv.tm_mday, tmv.tm_hour, tmv.tm_min, tmv.tm_sec, ms);
	return buf;
}

std::string toJson(const KillEvent &ev, bool includeEvidence, int indent)
{
	nlohmann::ordered_json j;
	j["schemaVersion"] = ev.schemaVersion;
	j["event"] = eventTypeId(ev.type);
	j["game"] = ev.game;
	j["profile"] = ev.profile;
	j["target"] = ev.target;
	// Round confidence to 3 decimals for stable output.
	j["confidence"] = static_cast<double>(static_cast<int64_t>(ev.confidence * 1000.0 + 0.5)) / 1000.0;
	j["sessionKills"] = ev.sessionKills;
	j["killStreak"] = ev.killStreak;
	j["bestStreak"] = ev.bestStreak;
	j["sessionDeaths"] = ev.sessionDeaths;
	j["sequence"] = ev.sequence;
	j["timestamp"] = ev.timestamp;
	j["timestampMs"] = ev.timestampMs;
	j["detector"] = ev.detector;
	j["test"] = ev.test;
	if (!ev.reason.empty())
		j["reason"] = ev.reason;
	if (includeEvidence && !ev.evidence.empty()) {
		auto arr = nlohmann::ordered_json::array();
		for (const auto &e : ev.evidence) {
			nlohmann::ordered_json item;
			item["detector"] = e.detector;
			item["kind"] = e.kind;
			item["weight"] = e.weight;
			item["score"] = e.score;
			item["contribution"] = e.contribution;
			item["active"] = e.active;
			arr.push_back(std::move(item));
		}
		j["evidence"] = std::move(arr);
	}
	return j.dump(indent, ' ', false, nlohmann::ordered_json::error_handler_t::replace);
}

bool fromJson(const std::string &json, KillEvent &out, std::string *error)
{
	try {
		const auto j = nlohmann::json::parse(json);
		KillEvent ev;
		ev.schemaVersion = j.value("schemaVersion", 1);
		const auto type = parseEventType(j.value("event", std::string("kill")));
		if (!type) {
			if (error)
				*error = "unknown event type";
			return false;
		}
		ev.type = *type;
		ev.game = j.value("game", std::string());
		ev.profile = j.value("profile", std::string());
		ev.target = j.value("target", std::string());
		ev.confidence = j.value("confidence", 0.0);
		ev.sessionKills = j.value("sessionKills", int64_t{0});
		ev.killStreak = j.value("killStreak", int64_t{0});
		ev.bestStreak = j.value("bestStreak", int64_t{0});
		ev.sessionDeaths = j.value("sessionDeaths", int64_t{0});
		ev.sequence = j.value("sequence", int64_t{0});
		ev.timestamp = j.value("timestamp", std::string());
		ev.timestampMs = j.value("timestampMs", int64_t{0});
		ev.detector = j.value("detector", std::string());
		ev.test = j.value("test", false);
		ev.reason = j.value("reason", std::string());
		if (j.contains("evidence") && j["evidence"].is_array()) {
			for (const auto &item : j["evidence"]) {
				EvidenceItem e;
				e.detector = item.value("detector", std::string());
				e.kind = item.value("kind", std::string());
				e.weight = item.value("weight", 0.0);
				e.score = item.value("score", 0.0);
				e.contribution = item.value("contribution", 0.0);
				e.active = item.value("active", false);
				ev.evidence.push_back(std::move(e));
			}
		}
		out = std::move(ev);
		return true;
	} catch (const std::exception &e) {
		if (error)
			*error = e.what();
		return false;
	}
}

} // namespace killsense
