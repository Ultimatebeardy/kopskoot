// SPDX-License-Identifier: GPL-2.0-or-later
#include "detectors.hpp"

#include "killsense/fsutil.hpp"
#include "killsense/logging.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace killsense::detail {

TemplateDetector::TemplateDetector(const DetectorSpec &spec) : spec_(spec)
{
	const std::string path = pathToUtf8(spec_.resolvedFile);
	if (!loadImageFile(path, original_, PixelFormat::Gray8)) {
		error_ = "cannot load template image '" + path + "'";
		KS_LOG_WARN("detector '%s': %s", spec_.id.c_str(), error_.c_str());
		return;
	}
	if (original_.width() < 4 || original_.height() < 4) {
		error_ = "template image is smaller than 4x4 pixels";
		return;
	}
	loaded_ = true;
}

TemplateMatcher &TemplateDetector::matcherFor(double effectiveScale)
{
	const int key = static_cast<int>(std::lround(effectiveScale * 100.0));
	auto it = matchers_.find(key);
	if (it != matchers_.end())
		return it->second;
	// Bound the cache in case the source resolution keeps changing.
	if (matchers_.size() >= 16)
		matchers_.clear();
	TemplateMatcher &m = matchers_[key];
	const double s = key / 100.0;
	if (key == 100) {
		m.setTemplate(original_);
	} else {
		Image scaled;
		resize(original_, std::max(4, static_cast<int>(std::lround(original_.width() * s))),
		       std::max(4, static_cast<int>(std::lround(original_.height() * s))), scaled);
		m.setTemplate(scaled);
	}
	return m;
}

void TemplateDetector::process(const DetectorContext &ctx, DetectorOutput &out)
{
	out.available = loaded_;
	if (!loaded_) {
		out.detail = error_;
		return;
	}
	const auto t0 = std::chrono::steady_clock::now();
	double best = 0.0;
	std::vector<TemplateMatch> bestMatches;
	for (double scale : spec_.scales) {
		const double eff = scale * ctx.region->pxPerReference;
		TemplateMatcher &m = matcherFor(eff);
		double b = 0.0;
		auto matches = m.match(*ctx.gray, spec_.threshold, spec_.maxInstances, b);
		if (b > best) {
			best = b;
		}
		if (matches.size() > bestMatches.size() || (matches.size() == bestMatches.size() && !matches.empty() &&
							    matches[0].score > bestMatches[0].score))
			bestMatches = std::move(matches);
	}
	out.ran = true;
	out.instances = static_cast<int>(bestMatches.size());
	for (const auto &m : bestMatches)
		out.boxes.push_back(m.box);
	out.active = !bestMatches.empty();
	// Continuous evidence: ramps from (threshold - 0.15) to threshold.
	const double lo = spec_.threshold - 0.15;
	out.score = std::clamp((best - lo) / (spec_.threshold - lo), 0.0, 1.0);
	char buf[96];
	std::snprintf(buf, sizeof(buf), "ncc=%.3f matches=%d", best, out.instances);
	out.detail = buf;
	out.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

} // namespace killsense::detail
