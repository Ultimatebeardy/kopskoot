// SPDX-License-Identifier: GPL-2.0-or-later
#include "test-helpers.hpp"

#include "killsense/event-bus.hpp"
#include "killsense/event.hpp"
#include "killsense/session.hpp"

#include <nlohmann/json.hpp>

#include <atomic>

using namespace killsense;

namespace {
KillEvent make(EventType t)
{
	KillEvent e;
	e.type = t;
	e.profile = "sample-killfeed";
	e.game = "Sample";
	e.confidence = 0.9;
	e.detector = "vision";
	return e;
}
} // namespace

TEST(Session, CountsKillsStreaksAndDeaths)
{
	SessionTracker s;
	for (int i = 0; i < 3; ++i) {
		auto e = make(EventType::Kill);
		s.record(e);
		EXPECT_EQ(e.sessionKills, i + 1);
		EXPECT_EQ(e.killStreak, i + 1);
	}
	auto hs = make(EventType::Headshot);
	s.record(hs);
	EXPECT_EQ(hs.sessionKills, 4);
	auto down = make(EventType::EnemyDown); // a knock is not a kill
	s.record(down);
	EXPECT_EQ(down.sessionKills, 4);
	auto death = make(EventType::Death);
	s.record(death);
	EXPECT_EQ(death.killStreak, 0);
	EXPECT_EQ(death.sessionDeaths, 1);
	EXPECT_EQ(death.bestStreak, 4);
	auto k = make(EventType::Kill);
	s.record(k);
	EXPECT_EQ(k.killStreak, 1);
	EXPECT_EQ(k.sessionKills, 5);
	EXPECT_EQ(k.sequence, 7);

	auto reset = s.reset("sample-killfeed", "Sample");
	EXPECT_EQ(reset.type, EventType::SessionReset);
	EXPECT_EQ(s.stats().kills, 0);
	EXPECT_EQ(s.stats().bestStreak, 0);
}

TEST(Event, SerializationMatchesSchema)
{
	KillEvent e = make(EventType::Kill);
	e.confidence = 0.94049;
	e.sessionKills = 17;
	e.killStreak = 4;
	e.timestampMs = 1790000000123;
	e.timestamp = isoTimestampUtc(e.timestampMs);
	e.evidence.push_back({"kill_icon", "template", 0.45, 1.0, 0.45, true});
	const std::string text = toJson(e, true);
	const auto j = nlohmann::json::parse(text);
	EXPECT_EQ(j["schemaVersion"], 1);
	EXPECT_EQ(j["event"], "kill");
	EXPECT_EQ(j["profile"], "sample-killfeed");
	EXPECT_DOUBLE_EQ(j["confidence"].get<double>(), 0.94);
	EXPECT_EQ(j["sessionKills"], 17);
	EXPECT_EQ(j["killStreak"], 4);
	EXPECT_EQ(j["detector"], "vision");
	EXPECT_EQ(j["timestamp"], "2026-09-21T14:13:20.123Z");
	EXPECT_EQ(j["evidence"][0]["detector"], "kill_icon");

	KillEvent back;
	ASSERT_TRUE(fromJson(text, back));
	EXPECT_EQ(back.type, EventType::Kill);
	EXPECT_EQ(back.sessionKills, 17);
	EXPECT_EQ(back.evidence.size(), 1u);

	EXPECT_EQ(nlohmann::json::parse(toJson(e, false)).count("evidence"), 0u);
}

TEST(Event, TypeNames)
{
	EXPECT_STREQ(eventTypeId(EventType::DoubleKill), "double_kill");
	EXPECT_STREQ(eventTypeVendorName(EventType::VehicleDestroyed), "VehicleDestroyed");
	EXPECT_EQ(parseEventType("boss_kill"), EventType::BossKill);
	EXPECT_EQ(parseEventType("MultiKill"), EventType::MultiKill);
	EXPECT_FALSE(parseEventType("teabag").has_value());
	EXPECT_FALSE(countsAsKill(EventType::DoubleKill)); // announcement, not an extra kill
	KillEvent bad;
	EXPECT_FALSE(fromJson("{\"event\":\"nope\"}", bad));
	EXPECT_FALSE(fromJson("garbage", bad));
}

TEST(EventBus, DeliversOnDispatchThreadAndSurvivesThrowingSubscribers)
{
	EventBus bus(8);
	bus.start();
	std::atomic<int> got{0};
	bus.subscribe([](const KillEvent &) { throw std::runtime_error("bad subscriber"); });
	auto id = bus.subscribe([&](const KillEvent &) { ++got; });
	for (int i = 0; i < 5; ++i)
		EXPECT_TRUE(bus.publish(make(EventType::Kill)));
	bus.flush();
	EXPECT_EQ(got.load(), 5);
	bus.unsubscribe(id);
	bus.publish(make(EventType::Kill));
	bus.flush();
	EXPECT_EQ(got.load(), 5);
	bus.stop();
	EXPECT_FALSE(bus.publish(make(EventType::Kill)));
}

TEST(Session, FormatCounter)
{
	SessionStats s;
	s.kills = 7;
	s.deaths = 2;
	s.streak = 3;
	s.bestStreak = 5;
	EXPECT_EQ(formatCounter("K {kills} D {deaths} S {streak}/{best} KD {kd}", s), "K 7 D 2 S 3/5 KD 3.50");
	s.deaths = 0;
	EXPECT_EQ(kdRatioText(s), "7.00");
}
