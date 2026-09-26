// SPDX-License-Identifier: GPL-2.0-or-later
#include "test-helpers.hpp"

#include "killsense/fusion.hpp"
#include "killsense/killfeed.hpp"
#include "killsense/profile.hpp"

using namespace killsense;

namespace {
OcrLine line(std::initializer_list<std::pair<const char *, PixelRect>> words)
{
	OcrLine l;
	for (const auto &w : words) {
		l.words.push_back(OcrWord{w.first, w.second});
		if (!l.text.empty())
			l.text += ' ';
		l.text += w.first;
	}
	return l;
}
} // namespace

TEST(KillFeed, ActorWhenAnotherNameFollows)
{
	std::vector<OcrLine> lines{line({{"UltimateBeardy", {0, 0, 140, 20}}, {">", {150, 0, 10, 20}},
					 {"Jbrnss", {170, 0, 70, 20}}})};
	auto m = findNameInFeed(lines, "UltimateBeardy", 0.75);
	ASSERT_EQ(m.size(), 1u);
	EXPECT_EQ(m[0].role, FeedRole::Actor);
}

TEST(KillFeed, VictimWhenLastOnLineAndOcrConfusesLetters)
{
	// "UItimateBeardy" (capital I for l) plus a club tag before the killer.
	std::vector<OcrLine> lines{line({{"LAT3R", {0, 0, 50, 20}}, {"hageegi", {60, 0, 70, 20}},
					 {"UItimateBeardy", {170, 0, 140, 20}}})};
	auto m = findNameInFeed(lines, "UltimateBeardy", 0.75);
	ASSERT_EQ(m.size(), 1u);
	EXPECT_EQ(m[0].role, FeedRole::Victim);
}

TEST(KillFeed, SplitLinesUseGeometry)
{
	// OCR split the entry at the weapon icon: two lines at the same height.
	std::vector<OcrLine> lines{line({{"UltimateBeardy", {0, 40, 140, 20}}}),
				   line({{"Jbrnss", {200, 38, 70, 20}}}),
				   line({{"Benji", {60, 80, 50, 20}}, {"BalletPlace", {150, 78, 120, 20}}})};
	auto m = findNameInFeed(lines, "UltimateBeardy", 0.75);
	ASSERT_EQ(m.size(), 1u);
	EXPECT_EQ(m[0].role, FeedRole::Actor);

	std::vector<OcrLine> victim{line({{"hageegi", {0, 0, 70, 20}}}), line({{"UltimateBeardy", {130, 60, 140, 20}}}),
				    line({{"Benji", {60, 30, 50, 20}}, {"BalletPlace", {150, 30, 120, 20}}})};
	m = findNameInFeed(victim, "UltimateBeardy", 0.75);
	ASSERT_EQ(m.size(), 1u);
	EXPECT_EQ(m[0].role, FeedRole::Victim);
}

TEST(KillFeed, OtherNamesDoNotMatch)
{
	std::vector<OcrLine> lines{line({{"BlakkMerl1", {0, 0, 100, 20}}, {"NotedDevice", {150, 0, 110, 20}}})};
	EXPECT_TRUE(findNameInFeed(lines, "UltimateBeardy", 0.75).empty());
}

TEST(KillFeed, MultiWordGamertag)
{
	std::vector<OcrLine> lines{line({{"Big", {0, 0, 30, 20}}, {"Beardy", {35, 0, 60, 20}}, {"Jbrnss", {150, 0, 70, 20}}})};
	auto m = findNameInFeed(lines, "Big Beardy", 0.8);
	ASSERT_EQ(m.size(), 1u);
	EXPECT_EQ(m[0].role, FeedRole::Actor);
}

namespace {
const char *kPersonalProfile = R"({
  "schemaVersion": 1, "id": "pers", "displayName": "P",
  "regions": [{"id": "r", "rect": {"x": 0, "y": 0, "w": 1, "h": 1}}],
  "detectors": [
    {"id": "name", "type": "ocr", "region": "r", "keywords": ["{gamertag}"], "patterns": ["^{gamertag}$"]},
    {"id": "plate", "type": "template", "region": "r", "userTemplate": "own-plate"},
    {"id": "bar", "type": "color", "region": "r", "hsv": {"hMin": 0, "hMax": 20}, "vetoWhenActive": true}
  ],
  "targets": [{"id": "kill", "event": "kill", "threshold": 0.5, "minSignals": 1,
               "signals": [{"detector": "name", "weight": 1}, {"detector": "plate", "weight": 1}],
               "requireAll": ["plate"], "requireAny": ["name"]}]
})";
}

TEST(Personalization, SubstitutesGamertagAndEscapesPatterns)
{
	auto parsed = parseProfileJson(kPersonalProfile, kstest::dataDir());
	ASSERT_TRUE(parsed.ok) << (parsed.errors.empty() ? "" : parsed.errors.front());
	EXPECT_TRUE(profileUsesGamertag(*parsed.profile));
	ASSERT_EQ(parsed.profile->targets.front().requireAll.size(), 1u);
	EXPECT_TRUE(parsed.profile->findDetector("bar")->vetoWhenActive);

	Personalization pers;
	pers.gamertag = "  x.Beard+y ";
	auto r = personalizeProfile(*parsed.profile, pers);
	EXPECT_FALSE(r.needsGamertag);
	const auto *d = r.profile->findDetector("name");
	EXPECT_EQ(d->keywords.front(), "x.Beard+y");
	EXPECT_EQ(d->patterns.front(), "^x\\.Beard\\+y$");
	ASSERT_EQ(r.missingUserTemplates.size(), 1u);
	EXPECT_EQ(r.missingUserTemplates.front(), "own-plate");
}

TEST(Personalization, MissingGamertagDisablesDetector)
{
	auto parsed = parseProfileJson(kPersonalProfile, kstest::dataDir());
	ASSERT_TRUE(parsed.ok);
	auto r = personalizeProfile(*parsed.profile, Personalization{});
	EXPECT_TRUE(r.needsGamertag);
	EXPECT_FALSE(r.profile->findDetector("name")->enabled);
}

TEST(Personalization, UserTemplateResolvesToCapturedFile)
{
	auto parsed = parseProfileJson(kPersonalProfile, kstest::dataDir());
	ASSERT_TRUE(parsed.ok);
	const auto dir = std::filesystem::temp_directory_path() / "ks-user-templates-test";
	std::filesystem::create_directories(dir / "pers");
	const auto file = userTemplatePath(dir, "pers", "own-plate");
	Image img = kstest::solid(20, 10, 200, 200, 200);
	ASSERT_TRUE(savePng(pathToUtf8(file), img));
	Personalization pers;
	pers.gamertag = "me";
	pers.userTemplateDir = dir;
	auto r = personalizeProfile(*parsed.profile, pers);
	EXPECT_TRUE(r.missingUserTemplates.empty());
	EXPECT_EQ(r.profile->findDetector("plate")->resolvedFile, file);
	std::filesystem::remove_all(dir);
}

TEST(Fusion, RequireAllNeedsEveryDetectorAvailableAndActive)
{
	auto parsed = parseProfileJson(kPersonalProfile, kstest::dataDir());
	ASSERT_TRUE(parsed.ok);
	const TargetSpec &t = parsed.profile->targets.front();
	std::vector<DetectorOutput> outs(2);
	outs[0].detectorId = "name";
	outs[0].kind = DetectorKind::Ocr;
	outs[0].score = 1.0;
	outs[0].active = true;
	outs[1].detectorId = "plate";
	outs[1].available = false;
	auto r = fuseTarget(t, outs);
	EXPECT_FALSE(r.passes);
	EXPECT_FALSE(r.requireAllSatisfied);
	outs[1].available = true;
	outs[1].active = true;
	outs[1].score = 1.0;
	r = fuseTarget(t, outs);
	EXPECT_TRUE(r.passes) << r.reason;
}

#include "killsense/user-template.hpp"

TEST(UserTemplate, CropsWhiteTextAndScalesToReference)
{
	Image region = kstest::solid(240, 70, 60, 70, 90);
	// "Text": 20 thin white strokes, 100 px wide, 20 px tall.
	Image stroke = kstest::solid(2, 20, 250, 250, 250);
	for (int i = 0; i < 20; ++i)
		kstest::paste(region, stroke, 30 + i * 5 + (i == 19 ? 3 : 0), 30);
	// A solid bar above it (like a health bar) must not be included.
	kstest::paste(region, kstest::solid(200, 8, 250, 250, 250), 10, 4);
	auto r = cropNameTemplate(region, 1.0);
	ASSERT_TRUE(r.ok) << r.message;
	EXPECT_EQ(r.box.x, 27);
	EXPECT_EQ(r.box.y, 27);
	EXPECT_EQ(r.image.width(), 106);
	// 720p capture (0.667 px per reference px) is stored at 1080p scale.
	auto s = cropNameTemplate(region, 2.0 / 3.0);
	ASSERT_TRUE(s.ok);
	EXPECT_EQ(s.image.width(), 159);
}

TEST(UserTemplate, RejectsBrightOrEmptyRegions)
{
	EXPECT_FALSE(cropNameTemplate(kstest::solid(200, 60, 40, 40, 40), 1.0).ok);
	EXPECT_FALSE(cropNameTemplate(kstest::solid(200, 60, 240, 240, 240), 1.0).ok);
}
