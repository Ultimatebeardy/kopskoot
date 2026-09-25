// REALM KillSense - OCR-tolerant text matching helpers.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace killsense {

// Upper-cases ASCII, maps non-alphanumerics to single spaces and trims.
std::string normalizeText(std::string_view text);

// Levenshtein distance on bytes (inputs are expected to be normalized).
int editDistance(std::string_view a, std::string_view b);
// 1 - distance / max(len). Two empty strings are identical (1.0).
double textSimilarity(std::string_view a, std::string_view b);

// True if two normalized OCR messages are the same message despite OCR noise.
// Compared word by word (every word must reach `threshold`), so messages that
// share a long prefix but name a different player ("... RAIDER" vs
// "... WARDEN") are different. Falls back to whole-string similarity with a
// stricter threshold when OCR split or merged words.
bool sameMessage(std::string_view a, std::string_view b, double threshold);

// Best similarity of `needle` against any window of the same word count in
// `haystack` (both normalized). Allows OCR mistakes such as "ELlMINATED".
double bestPhraseSimilarity(std::string_view haystackNormalized, std::string_view needleNormalized);

struct CompiledPatterns {
	std::vector<std::regex> regexes;
	std::vector<std::string> sources;
};
// Case-insensitive ECMAScript patterns. Invalid patterns are skipped.
CompiledPatterns compilePatterns(const std::vector<std::string> &patterns);
bool anyPatternMatches(const CompiledPatterns &patterns, const std::string &text, std::string *matched = nullptr);

} // namespace killsense
