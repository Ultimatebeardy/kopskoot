// SPDX-License-Identifier: GPL-2.0-or-later
#include "killsense/text-match.hpp"

#include <algorithm>
#include <sstream>

namespace killsense {

std::string normalizeText(std::string_view text)
{
	std::string out;
	out.reserve(text.size());
	bool pendingSpace = false;
	for (unsigned char c : text) {
		const bool alnum = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
				   c >= 0x80;
		if (alnum) {
			if (pendingSpace && !out.empty())
				out.push_back(' ');
			pendingSpace = false;
			out.push_back(static_cast<char>((c >= 'a' && c <= 'z') ? c - 32 : c));
		} else {
			pendingSpace = true;
		}
	}
	return out;
}

int editDistance(std::string_view a, std::string_view b)
{
	if (a.size() < b.size())
		std::swap(a, b);
	std::vector<int> prev(b.size() + 1), cur(b.size() + 1);
	for (size_t j = 0; j <= b.size(); ++j)
		prev[j] = static_cast<int>(j);
	for (size_t i = 1; i <= a.size(); ++i) {
		cur[0] = static_cast<int>(i);
		for (size_t j = 1; j <= b.size(); ++j) {
			const int sub = prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1);
			cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, sub});
		}
		std::swap(prev, cur);
	}
	return prev[b.size()];
}

double textSimilarity(std::string_view a, std::string_view b)
{
	const size_t m = std::max(a.size(), b.size());
	if (m == 0)
		return 1.0;
	return 1.0 - static_cast<double>(editDistance(a, b)) / static_cast<double>(m);
}

namespace {
std::vector<std::string> splitWords(std::string_view s)
{
	std::vector<std::string> words;
	std::string cur;
	for (char c : s) {
		if (c == ' ') {
			if (!cur.empty())
				words.push_back(std::move(cur));
			cur.clear();
		} else {
			cur.push_back(c);
		}
	}
	if (!cur.empty())
		words.push_back(std::move(cur));
	return words;
}
} // namespace

double bestPhraseSimilarity(std::string_view haystack, std::string_view needle)
{
	if (needle.empty())
		return 0.0;
	if (haystack.find(needle) != std::string_view::npos)
		return 1.0;
	const auto hw = splitWords(haystack);
	const auto nw = splitWords(needle);
	if (hw.empty() || nw.empty())
		return 0.0;
	double best = 0.0;
	const size_t n = nw.size();
	// Compare windows of n-1..n+1 words to tolerate split/merged OCR tokens.
	for (size_t len = (n > 1 ? n - 1 : 1); len <= n + 1; ++len) {
		if (len > hw.size())
			break;
		for (size_t i = 0; i + len <= hw.size(); ++i) {
			std::string window;
			for (size_t k = 0; k < len; ++k) {
				if (k)
					window.push_back(' ');
				window += hw[i + k];
			}
			best = std::max(best, textSimilarity(window, needle));
			if (best >= 1.0)
				return 1.0;
		}
	}
	return best;
}

bool sameMessage(std::string_view a, std::string_view b, double threshold)
{
	if (a == b)
		return true;
	if (a.empty() || b.empty())
		return false;
	const auto wa = splitWords(a);
	const auto wb = splitWords(b);
	if (wa.size() == wb.size()) {
		for (size_t i = 0; i < wa.size(); ++i)
			if (textSimilarity(wa[i], wb[i]) < threshold)
				return false;
		return true;
	}
	return textSimilarity(a, b) >= std::max(threshold, 0.9);
}

CompiledPatterns compilePatterns(const std::vector<std::string> &patterns)
{
	CompiledPatterns out;
	for (const auto &p : patterns) {
		try {
			out.regexes.emplace_back(p, std::regex::ECMAScript | std::regex::icase | std::regex::optimize);
			out.sources.push_back(p);
		} catch (const std::regex_error &) {
			// Validated at profile load; skip defensively.
		}
	}
	return out;
}

bool anyPatternMatches(const CompiledPatterns &patterns, const std::string &text, std::string *matched)
{
	for (size_t i = 0; i < patterns.regexes.size(); ++i) {
		try {
			if (std::regex_search(text, patterns.regexes[i])) {
				if (matched)
					*matched = patterns.sources[i];
				return true;
			}
		} catch (const std::regex_error &) {
			// e.g. complexity errors on pathological input
		}
	}
	return false;
}

} // namespace killsense
