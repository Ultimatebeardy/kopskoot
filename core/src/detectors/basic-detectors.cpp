// SPDX-License-Identifier: GPL-2.0-or-later
// Colour/layout and HUD visual-change detectors, plus the detector factory.
#include "detectors.hpp"

#include "killsense/logging.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace killsense::detail {

void ColorDetector::process(const DetectorContext &ctx, DetectorOutput &out)
{
	const auto t0 = std::chrono::steady_clock::now();
	const Image &img = ctx.region->rgba;
	const auto &r = spec_.hsv;
	int64_t total = 0, hits = 0;
	// Subsample every 2nd pixel in each direction: plenty for coverage ratios.
	for (int y = 0; y < img.height(); y += 2) {
		const uint8_t *row = img.row(y);
		for (int x = 0; x < img.width(); x += 2) {
			const uint8_t *p = row + x * 4;
			double h, s, v;
			rgbToHsv(p[0], p[1], p[2], h, s, v);
			const bool hueOk = r.hMin <= r.hMax ? (h >= r.hMin && h <= r.hMax)
							    : (h >= r.hMin || h <= r.hMax);
			if (hueOk && s >= r.sMin && s <= r.sMax && v >= r.vMin && v <= r.vMax)
				++hits;
			++total;
		}
	}
	const double frac = total > 0 ? static_cast<double>(hits) / total : 0.0;
	out.ran = true;
	if (frac > spec_.maxFraction) {
		out.score = 0.0;
		out.active = false;
	} else {
		out.score = spec_.minFraction > 0.0 ? std::min(1.0, frac / spec_.minFraction) : (frac > 0 ? 1.0 : 0.0);
		out.active = frac >= spec_.minFraction && frac > 0.0;
	}
	char buf[80];
	std::snprintf(buf, sizeof(buf), "coverage=%.2f%%", frac * 100.0);
	out.detail = buf;
	out.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

void HudChangeDetector::reset()
{
	havePrev_ = false;
	lastChangeMs_ = -1000000;
}

void HudChangeDetector::process(const DetectorContext &ctx, DetectorOutput &out)
{
	const auto t0 = std::chrono::steady_clock::now();
	const Image &g = *ctx.gray;
	// Compare at a fixed small width so the threshold is resolution independent.
	const int w = std::min(96, std::max(1, g.width()));
	const int h = std::max(1, static_cast<int>(std::lround(g.height() * (static_cast<double>(w) / g.width()))));
	resize(g, w, h, cur_);
	double diff = 0.0;
	if (havePrev_ && prev_.width() == cur_.width() && prev_.height() == cur_.height()) {
		diff = meanAbsDiff(prev_, cur_);
		if (diff >= spec_.changeThreshold)
			lastChangeMs_ = ctx.timeMs;
	}
	std::swap(prev_, cur_);
	havePrev_ = true;

	const int64_t since = ctx.timeMs - lastChangeMs_;
	out.ran = true;
	if (spec_.holdMs <= 0)
		out.score = since == 0 ? 1.0 : 0.0;
	else
		out.score = since >= 0 && since <= spec_.holdMs ? 1.0 - static_cast<double>(since) / (spec_.holdMs + 1)
								: 0.0;
	out.active = out.score > 0.0;
	char buf[96];
	std::snprintf(buf, sizeof(buf), "diff=%.1f lastChange=%lldms", diff,
		      static_cast<long long>(since > 999999 ? -1 : since));
	out.detail = buf;
	out.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

} // namespace killsense::detail

namespace killsense {

std::unique_ptr<IDetector> createDetector(const DetectorSpec &spec, const DetectorDeps &deps)
{
	try {
		switch (spec.kind) {
		case DetectorKind::Template:
			return std::make_unique<detail::TemplateDetector>(spec);
		case DetectorKind::Ocr:
			return std::make_unique<detail::OcrDetector>(spec, deps.ocr);
		case DetectorKind::Color:
			return std::make_unique<detail::ColorDetector>(spec);
		case DetectorKind::HudChange:
			return std::make_unique<detail::HudChangeDetector>(spec);
		case DetectorKind::Onnx:
			return std::make_unique<detail::UnavailableDetector>(
				spec, "ONNX inference is planned for Milestone 4");
		}
	} catch (const std::exception &e) {
		KS_LOG_ERROR("creating detector '%s' failed: %s", spec.id.c_str(), e.what());
	}
	return std::make_unique<detail::UnavailableDetector>(spec, "detector could not be created");
}

} // namespace killsense
