// SPDX-License-Identifier: GPL-2.0-or-later
#include "killsense/user-template.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace killsense {

TemplateCrop cropNameTemplate(const Image &rgba, double pxPerReference)
{
	TemplateCrop r;
	if (rgba.empty() || rgba.format() != PixelFormat::RGBA8) {
		r.message = "no image of the name plate area yet - start detection and try again";
		return r;
	}
	const int w = rgba.width(), h = rgba.height();
	// Light, unsaturated pixels = white HUD text.
	std::vector<uint8_t> mask(static_cast<size_t>(w) * h, 0);
	std::vector<int> rowCount(h, 0), rowTransitions(h, 0);
	int total = 0;
	for (int y = 0; y < h; ++y) {
		const uint8_t *p = rgba.row(y);
		uint8_t prev = 0;
		for (int x = 0; x < w; ++x, p += 4) {
			const int mx = std::max({p[0], p[1], p[2]}), mn = std::min({p[0], p[1], p[2]});
			const uint8_t on = (mx >= 185 && (mx - mn) <= 60) ? 1 : 0;
			mask[static_cast<size_t>(y) * w + x] = on;
			rowCount[y] += on;
			rowTransitions[y] += on != prev;
			prev = on;
			total += on;
		}
	}
	// Text rows have thin strokes: many on/off transitions per lit pixel.
	// Solid shapes (a health bar) have few, so they are excluded; then rows are
	// grouped into bands and the most text-like band is kept.
	auto textRow = [&](int y) {
		return rowCount[y] >= 2 && rowTransitions[y] * 5 >= rowCount[y];
	};
	int y0 = -1, y1 = -1, bestScore = 0;
	for (int y = 0; y < h;) {
		if (!textRow(y)) {
			++y;
			continue;
		}
		int a = y, b = y, gap = 0, score = 0;
		for (; y < h && gap <= 2; ++y) {
			if (textRow(y)) {
				b = y;
				gap = 0;
				score += rowTransitions[y];
			} else {
				++gap;
			}
		}
		if (b - a >= 4 && score > bestScore) {
			bestScore = score;
			y0 = a;
			y1 = b;
		}
	}
	int x0 = -1, x1 = -1;
	if (y0 >= 0) {
		for (int x = 0; x < w; ++x) {
			int c = 0;
			for (int y = y0; y <= y1; ++y)
				c += mask[static_cast<size_t>(y) * w + x];
			if (c >= 2) {
				if (x0 < 0)
					x0 = x;
				x1 = x;
			}
		}
	}
	if (x0 < 0 || y0 < 0 || x1 - x0 < 10 || y1 - y0 < 4) {
		r.message = "could not find your name text - make sure your name plate is on screen";
		return r;
	}
	const double fill = static_cast<double>(total) / (static_cast<double>(w) * h);
	if (fill > 0.45 || ((x1 - x0 + 1) > w * 0.98 && (y1 - y0 + 1) > h * 0.95)) {
		r.message = "the background behind your name is too bright - look at something darker and try again";
		return r;
	}
	const int pad = 3;
	x0 = std::max(0, x0 - pad);
	y0 = std::max(0, y0 - pad);
	x1 = std::min(w - 1, x1 + pad);
	y1 = std::min(h - 1, y1 + pad);
	r.box = PixelRect{x0, y0, x1 - x0 + 1, y1 - y0 + 1};
	Image crop(r.box.w, r.box.h, PixelFormat::RGBA8);
	for (int y = 0; y < r.box.h; ++y) {
		uint8_t *dst = crop.row(y);
		std::copy(rgba.row(y0 + y) + x0 * 4, rgba.row(y0 + y) + (x0 + r.box.w) * 4, dst);
		// Some capture sources deliver alpha = 0; the template must be opaque.
		for (int x = 0; x < r.box.w; ++x)
			dst[x * 4 + 3] = 255;
	}
	const double s = pxPerReference > 0.0 ? 1.0 / pxPerReference : 1.0;
	if (std::abs(s - 1.0) > 0.02) {
		const int tw = std::max(4, static_cast<int>(std::lround(crop.width() * s)));
		const int th = std::max(4, static_cast<int>(std::lround(crop.height() * s)));
		resize(crop, tw, th, r.image);
	} else {
		r.image = std::move(crop);
	}
	r.ok = true;
	r.message = "name plate captured";
	return r;
}

} // namespace killsense
