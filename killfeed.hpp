// REALM KillSense - kill-feed name/role matching.
// Given OCR lines of a kill feed ("KILLER [icon] > VICTIM"), finds a player
// name (typically the user's gamertag) and decides whether that player is the
// actor (killer: another name follows on the same line) or the victim (the
// name is the last one on its line / right-aligned edge of the feed).
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "killsense/geometry.hpp"
#include "killsense/ocr.hpp"

#include <string>
#include <vector>

namespace killsense {

enum class FeedRole { Unknown, Actor, Victim };

const char *feedRoleId(FeedRole role);

struct FeedMatch {
	FeedRole role = FeedRole::Unknown;
	PixelRect box; // union of the matched word boxes
	double similarity = 0.0;
	int line = 0;
};

// `fuzzy`: minimum similarity (0..1) between the name and OCR text, compared
// with spaces/punctuation removed and case-insensitively (OCR often reads
// "l" as "I"). Names may span up to three OCR words.
std::vector<FeedMatch> findNameInFeed(const std::vector<OcrLine> &lines, const std::string &name, double fuzzy);

} // namespace killsense
