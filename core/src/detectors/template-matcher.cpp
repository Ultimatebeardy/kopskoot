// SPDX-License-Identifier: GPL-2.0-or-later
// Normalized cross-correlation template matching.
//   ncc(x,y) = sum(I * T') / sqrt(varsum(I_window) * sum(T'^2)),  T' = T - mean(T)
// Window sums of I and I^2 come from integral images, so only the correlation
// term costs O(template area) per position. A coarse pass on a downscaled
// pyramid level selects candidates that are refined at full resolution.
#include "killsense/detector.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace killsense {

namespace {

struct Integral {
	int w = 0, h = 0;
	std::vector<double> sum, sq;
	void build(const Image &g)
	{
		w = g.width();
		h = g.height();
		sum.assign(static_cast<size_t>(w + 1) * (h + 1), 0.0);
		sq.assign(sum.size(), 0.0);
		for (int y = 0; y < h; ++y) {
			const uint8_t *r = g.row(y);
			double rs = 0.0, rq = 0.0;
			for (int x = 0; x < w; ++x) {
				rs += r[x];
				rq += static_cast<double>(r[x]) * r[x];
				const size_t i = static_cast<size_t>(y + 1) * (w + 1) + (x + 1);
				sum[i] = sum[i - (w + 1)] + rs;
				sq[i] = sq[i - (w + 1)] + rq;
			}
		}
	}
	double rect(const std::vector<double> &t, int x, int y, int rw, int rh) const
	{
		const size_t W = static_cast<size_t>(w + 1);
		return t[(y + rh) * W + (x + rw)] - t[y * W + (x + rw)] - t[(y + rh) * W + x] + t[y * W + x];
	}
};

struct PreparedTemplate {
	int w = 0, h = 0;
	std::vector<double> zeroMean; // T - mean(T)
	double norm = 0.0;            // sqrt(sum(T'^2))
	void build(const Image &t)
	{
		w = t.width();
		h = t.height();
		const size_t n = static_cast<size_t>(w) * h;
		zeroMean.resize(n);
		double mean = 0.0;
		for (size_t i = 0; i < n; ++i)
			mean += t.data()[i];
		mean /= static_cast<double>(n ? n : 1);
		double s = 0.0;
		for (size_t i = 0; i < n; ++i) {
			zeroMean[i] = t.data()[i] - mean;
			s += zeroMean[i] * zeroMean[i];
		}
		norm = std::sqrt(s);
	}
};

double evalNcc(const Image &img, const Integral &ii, const PreparedTemplate &pt, int x, int y)
{
	if (pt.norm < 1e-6)
		return 0.0;
	const double n = static_cast<double>(pt.w) * pt.h;
	const double s = ii.rect(ii.sum, x, y, pt.w, pt.h);
	const double q = ii.rect(ii.sq, x, y, pt.w, pt.h);
	const double var = q - s * s / n;
	if (var < 1e-3 * n) // flat window
		return 0.0;
	double cross = 0.0;
	const double *tp = pt.zeroMean.data();
	for (int ty = 0; ty < pt.h; ++ty) {
		const uint8_t *row = img.row(y + ty) + x;
		for (int tx = 0; tx < pt.w; ++tx)
			cross += row[tx] * (*tp++);
	}
	return cross / (std::sqrt(var) * pt.norm);
}

struct Candidate {
	int x, y;
	double score;
};

} // namespace

double nccAt(const Image &gray, const Image &templ, int x, int y)
{
	if (gray.format() != PixelFormat::Gray8 || templ.format() != PixelFormat::Gray8)
		return 0.0;
	if (x < 0 || y < 0 || x + templ.width() > gray.width() || y + templ.height() > gray.height())
		return 0.0;
	Integral ii;
	ii.build(gray);
	PreparedTemplate pt;
	pt.build(templ);
	return evalNcc(gray, ii, pt, x, y);
}

void TemplateMatcher::setTemplate(const Image &templGray)
{
	if (templGray.format() != PixelFormat::Gray8) {
		toGray(templGray, templ_);
	} else {
		templ_ = templGray;
	}
	const int minSide = std::min(templ_.width(), templ_.height());
	coarseFactor_ = std::clamp(minSide / 12, 1, 4);
	if (coarseFactor_ > 1)
		resize(templ_, std::max(1, templ_.width() / coarseFactor_),
		       std::max(1, templ_.height() / coarseFactor_), templCoarse_);
	else
		templCoarse_.clear();
}

std::vector<TemplateMatch> TemplateMatcher::match(const Image &gray, double threshold, int maxInstances,
						  double &bestScore)
{
	bestScore = 0.0;
	std::vector<TemplateMatch> out;
	if (templ_.empty() || gray.empty() || gray.format() != PixelFormat::Gray8)
		return out;
	const int tw = templ_.width(), th = templ_.height();
	if (tw > gray.width() || th > gray.height())
		return out;

	PreparedTemplate full;
	full.build(templ_);
	Integral iiFull;
	iiFull.build(gray);

	std::vector<Candidate> cands;
	const int f = coarseFactor_;
	if (f > 1 && gray.width() / f >= templCoarse_.width() && gray.height() / f >= templCoarse_.height()) {
		resize(gray, gray.width() / f, gray.height() / f, coarse_);
		PreparedTemplate pc;
		pc.build(templCoarse_);
		Integral iic;
		iic.build(coarse_);
		const int mw = coarse_.width() - pc.w + 1, mh = coarse_.height() - pc.h + 1;
		std::vector<double> map(static_cast<size_t>(mw) * mh);
		for (int y = 0; y < mh; ++y)
			for (int x = 0; x < mw; ++x)
				map[static_cast<size_t>(y) * mw + x] = evalNcc(coarse_, iic, pc, x, y);
		const double coarseMin = std::max(0.3, threshold - 0.25);
		for (int y = 0; y < mh; ++y) {
			for (int x = 0; x < mw; ++x) {
				const double v = map[static_cast<size_t>(y) * mw + x];
				if (v < coarseMin)
					continue;
				bool isMax = true;
				for (int dy = -1; dy <= 1 && isMax; ++dy)
					for (int dx = -1; dx <= 1; ++dx) {
						const int nx = x + dx, ny = y + dy;
						if ((dx || dy) && nx >= 0 && ny >= 0 && nx < mw && ny < mh &&
						    map[static_cast<size_t>(ny) * mw + nx] > v) {
							isMax = false;
							break;
						}
					}
				if (isMax)
					cands.push_back({x, y, v});
			}
		}
		std::sort(cands.begin(), cands.end(),
			  [](const Candidate &a, const Candidate &b) { return a.score > b.score; });
		if (cands.size() > static_cast<size_t>(maxInstances * 4 + 4))
			cands.resize(static_cast<size_t>(maxInstances * 4 + 4));
		// Refine each candidate at full resolution.
		std::vector<Candidate> refined;
		for (const auto &c : cands) {
			Candidate best{0, 0, -1.0};
			const int cx = c.x * f, cy = c.y * f;
			for (int y = std::max(0, cy - f); y <= std::min(gray.height() - th, cy + f); ++y)
				for (int x = std::max(0, cx - f); x <= std::min(gray.width() - tw, cx + f); ++x) {
					const double v = evalNcc(gray, iiFull, full, x, y);
					if (v > best.score)
						best = {x, y, v};
				}
			if (best.score >= 0.0)
				refined.push_back(best);
		}
		cands = std::move(refined);
	} else {
		const int mw = gray.width() - tw + 1, mh = gray.height() - th + 1;
		std::vector<double> map(static_cast<size_t>(mw) * mh);
		for (int y = 0; y < mh; ++y)
			for (int x = 0; x < mw; ++x)
				map[static_cast<size_t>(y) * mw + x] = evalNcc(gray, iiFull, full, x, y);
		for (int y = 0; y < mh; ++y)
			for (int x = 0; x < mw; ++x) {
				const double v = map[static_cast<size_t>(y) * mw + x];
				if (v < std::max(0.3, threshold - 0.25))
					continue;
				bool isMax = true;
				for (int dy = -1; dy <= 1 && isMax; ++dy)
					for (int dx = -1; dx <= 1; ++dx) {
						const int nx = x + dx, ny = y + dy;
						if ((dx || dy) && nx >= 0 && ny >= 0 && nx < mw && ny < mh &&
						    map[static_cast<size_t>(ny) * mw + nx] > v) {
							isMax = false;
							break;
						}
					}
				if (isMax)
					cands.push_back({x, y, v});
			}
	}

	std::sort(cands.begin(), cands.end(), [](const Candidate &a, const Candidate &b) { return a.score > b.score; });
	for (const auto &c : cands)
		bestScore = std::max(bestScore, c.score);
	for (const auto &c : cands) {
		if (c.score < threshold || static_cast<int>(out.size()) >= maxInstances)
			break;
		const PixelRect box{c.x, c.y, tw, th};
		bool overlaps = false;
		for (const auto &m : out)
			if (iou(m.box, box) > 0.3) {
				overlaps = true;
				break;
			}
		if (!overlaps)
			out.push_back({box, c.score});
	}
	return out;
}

void rgbToHsv(uint8_t r8, uint8_t g8, uint8_t b8, double &h, double &s, double &v)
{
	const double r = r8 / 255.0, g = g8 / 255.0, b = b8 / 255.0;
	const double mx = std::max({r, g, b}), mn = std::min({r, g, b});
	const double d = mx - mn;
	v = mx;
	s = mx > 0.0 ? d / mx : 0.0;
	if (d <= 0.0) {
		h = 0.0;
		return;
	}
	if (mx == r)
		h = 60.0 * std::fmod((g - b) / d, 6.0);
	else if (mx == g)
		h = 60.0 * ((b - r) / d + 2.0);
	else
		h = 60.0 * ((r - g) / d + 4.0);
	if (h < 0.0)
		h += 360.0;
}

} // namespace killsense
