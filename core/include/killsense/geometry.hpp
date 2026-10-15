// KopSkoot - normalized / pixel rectangle helpers.
// Profiles store regions as normalized coordinates (0..1 of the source frame)
// so they stay resolution independent.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace killsense {

struct NormRect {
	double x = 0.0;
	double y = 0.0;
	double w = 0.0;
	double h = 0.0;

	bool isValid() const
	{
		return std::isfinite(x) && std::isfinite(y) && std::isfinite(w) && std::isfinite(h) && w > 0.0 &&
		       h > 0.0 && x >= 0.0 && y >= 0.0 && x + w <= 1.0 + 1e-9 && y + h <= 1.0 + 1e-9;
	}
	bool operator==(const NormRect &o) const { return x == o.x && y == o.y && w == o.w && h == o.h; }
};

struct PixelRect {
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;

	bool empty() const { return w <= 0 || h <= 0; }
	int right() const { return x + w; }
	int bottom() const { return y + h; }
	int64_t area() const { return empty() ? 0 : static_cast<int64_t>(w) * h; }
	bool operator==(const PixelRect &o) const { return x == o.x && y == o.y && w == o.w && h == o.h; }
};

// Converts a normalized rect to a pixel rect inside a width x height frame.
// The result is clamped to the frame and never negative. Edges are rounded
// outward so that tiny regions keep at least one pixel.
inline PixelRect toPixelRect(const NormRect &r, int width, int height)
{
	if (width <= 0 || height <= 0)
		return {};
	const double x0 = std::clamp(r.x, 0.0, 1.0) * width;
	const double y0 = std::clamp(r.y, 0.0, 1.0) * height;
	const double x1 = std::clamp(r.x + r.w, 0.0, 1.0) * width;
	const double y1 = std::clamp(r.y + r.h, 0.0, 1.0) * height;
	PixelRect p;
	p.x = static_cast<int>(std::floor(x0));
	p.y = static_cast<int>(std::floor(y0));
	const int rx = std::min(width, static_cast<int>(std::ceil(x1)));
	const int by = std::min(height, static_cast<int>(std::ceil(y1)));
	p.w = std::max(0, rx - p.x);
	p.h = std::max(0, by - p.y);
	return p;
}

inline NormRect toNormRect(const PixelRect &p, int width, int height)
{
	if (width <= 0 || height <= 0)
		return {};
	return NormRect{static_cast<double>(p.x) / width, static_cast<double>(p.y) / height,
			static_cast<double>(p.w) / width, static_cast<double>(p.h) / height};
}

inline PixelRect intersect(const PixelRect &a, const PixelRect &b)
{
	const int x0 = std::max(a.x, b.x);
	const int y0 = std::max(a.y, b.y);
	const int x1 = std::min(a.right(), b.right());
	const int y1 = std::min(a.bottom(), b.bottom());
	if (x1 <= x0 || y1 <= y0)
		return {};
	return PixelRect{x0, y0, x1 - x0, y1 - y0};
}

inline double iou(const PixelRect &a, const PixelRect &b)
{
	const PixelRect i = intersect(a, b);
	const double inter = static_cast<double>(i.area());
	const double uni = static_cast<double>(a.area() + b.area()) - inter;
	return uni > 0.0 ? inter / uni : 0.0;
}

// Parses "16:9", "21:9" or "2.37" style aspect strings. Returns <= 0 on failure.
inline double parseAspect(const char *text)
{
	if (!text)
		return 0.0;
	double a = 0.0, b = 0.0;
	char colon = 0;
	int n = 0;
#if defined(_MSC_VER)
	n = sscanf_s(text, "%lf%c%lf", &a, &colon, 1, &b);
#else
	n = std::sscanf(text, "%lf%c%lf", &a, &colon, &b);
#endif
	if (n == 3 && colon == ':' && b > 0.0)
		return a / b;
	if (n >= 1 && a > 0.0 && n != 2)
		return a;
	if (n == 2 && a > 0.0)
		return a;
	return 0.0;
}

} // namespace killsense
