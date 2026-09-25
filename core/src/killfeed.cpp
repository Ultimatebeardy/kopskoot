// SPDX-License-Identifier: GPL-2.0-or-later
#include "killsense/killfeed.hpp"

#include "killsense/text-match.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace killsense {

const char *feedRoleId(FeedRole role)
{
	switch (role) {
	case FeedRole::Actor:
		return "actor";
	case FeedRole::Victim:
		return "victim";
	case FeedRole::Unknown:
		break;
	}
	return "unknown";
}

namespace {

std::string squash(const std::string &s)
{
	std::string out;
	for (unsigned char c : s)
		if (std::isalnum(c))
			out += static_cast<char>(std::toupper(c));
	return out;
}

// A word that can be (part of) a player name: at least two alphanumerics.
// Arrows, weapon icons read as "-" / ">" etc. are ignored.
bool isNameLike(const OcrWord &w)
{
	return squash(w.text).size() >= 2;
}

PixelRect unite(const PixelRect &a, const PixelRect &b)
{
	if (a.w <= 0 || a.h <= 0)
		return b;
	const int x0 = std::min(a.x, b.x), y0 = std::min(a.y, b.y);
	const int x1 = std::max(a.x + a.w, b.x + b.w), y1 = std::max(a.y + a.h, b.y + b.h);
	return PixelRect{x0, y0, x1 - x0, y1 - y0};
}

} // namespace

std::vector<FeedMatch> findNameInFeed(const std::vector<OcrLine> &lines, const std::string &name, double fuzzy)
{
	std::vector<FeedMatch> out;
	const std::string target = squash(name);
	if (target.empty())
		return out;

	// Rightmost edge of any name-like word: the feed is right-aligned, so a
	// victim name ends there.
	int maxRight = 0;
	for (const auto &l : lines)
		for (const auto &w : l.words)
			if (isNameLike(w))
				maxRight = std::max(maxRight, w.box.x + w.box.w);

	for (size_t li = 0; li < lines.size(); ++li) {
		const auto &words = lines[li].words;
		size_t i = 0;
		while (i < words.size()) {
			// Best window of 1..3 words starting at i.
			double best = 0.0;
			size_t bestLen = 0;
			std::string joined;
			for (size_t n = 1; n <= 3 && i + n <= words.size(); ++n) {
				joined += squash(words[i + n - 1].text);
				if (joined.empty())
					continue;
				const double sim = textSimilarity(joined, target);
				if (sim > best) {
					best = sim;
					bestLen = n;
				}
				if (joined.size() > target.size() + 2)
					break;
			}
			if (bestLen == 0 || best < fuzzy) {
				++i;
				continue;
			}
			FeedMatch m;
			m.similarity = best;
			m.line = static_cast<int>(li);
			for (size_t k = i; k < i + bestLen; ++k)
				m.box = unite(m.box, words[k].box);
			bool nameRight = false, nameLeft = false;
			for (size_t k = 0; k < words.size(); ++k) {
				if (k >= i && k < i + bestLen)
					continue;
				if (!isNameLike(words[k]))
					continue;
				(k < i ? nameLeft : nameRight) = true;
			}
			if (nameRight) {
				m.role = FeedRole::Actor;
			} else if (nameLeft) {
				m.role = FeedRole::Victim;
			} else {
				// Alone on its OCR line (OCR split the entry at the icon gap):
				// look for a name on another line at the same height to the right.
				const int cy = m.box.y + m.box.h / 2;
				const int tol = std::max(4, m.box.h);
				bool right = false;
				for (size_t lj = 0; lj < lines.size() && !right; ++lj) {
					if (lj == li)
						continue;
					for (const auto &w : lines[lj].words) {
						if (!isNameLike(w))
							continue;
						const int wcy = w.box.y + w.box.h / 2;
						if (w.box.x >= m.box.x + m.box.w && std::abs(wcy - cy) <= tol) {
							right = true;
							break;
						}
					}
				}
				if (right)
					m.role = FeedRole::Actor;
				else if (m.box.w > 0 && m.box.x + m.box.w >= maxRight - std::max(4, m.box.h))
					m.role = FeedRole::Victim;
			}
			out.push_back(m);
			i += bestLen;
		}
	}
	return out;
}

} // namespace killsense
