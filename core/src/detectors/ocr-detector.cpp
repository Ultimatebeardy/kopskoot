// SPDX-License-Identifier: GPL-2.0-or-later
#include "detectors.hpp"

#include "killsense/logging.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace killsense::detail {

OcrDetector::OcrDetector(const DetectorSpec &spec, IOcrEngine *engine) : spec_(spec), engine_(engine)
{
	patterns_ = compilePatterns(spec_.patterns);
	for (const auto &k : spec_.keywords) {
		auto n = normalizeText(k);
		if (!n.empty())
			keywordsNorm_.push_back(std::move(n));
	}
	for (const auto &k : spec_.negativeWords) {
		auto n = normalizeText(k);
		if (!n.empty())
			negativeNorm_.push_back(std::move(n));
	}
}

std::string OcrDetector::unavailableReason() const
{
	if (!engine_)
		return "no OCR engine";
	if (!engine_->available())
		return std::string("OCR engine '") + engine_->name() + "' unavailable";
	return {};
}

void OcrDetector::reset()
{
	haveResult_ = false;
	lastHash_ = 0;
	lastRunMs_ = -1000000;
	lastRaw_.clear();
	lastNorm_.clear();
	lastBoxes_.clear();
}

namespace {
double keywordThreshold(const std::string &keyword, double fuzzy)
{
	// Very short words (e.g. "KILL") must match exactly: fuzzy matching them
	// would accept unrelated words such as "SKILL".
	return keyword.size() <= 5 ? 1.0 : fuzzy;
}
} // namespace

void OcrDetector::evaluate(DetectorOutput &out) const
{
	out.rawText = lastRaw_;
	out.text = lastNorm_;
	out.boxes = lastBoxes_;
	if (lastNorm_.empty()) {
		out.detail = "no text";
		return;
	}
	for (const auto &neg : negativeNorm_) {
		if (bestPhraseSimilarity(lastNorm_, neg) >= keywordThreshold(neg, spec_.fuzzy)) {
			out.veto = true;
			out.vetoReason = "negative phrase '" + neg + "'";
			break;
		}
	}
	double best = 0.0;
	std::string bestWhat;
	for (const auto &k : keywordsNorm_) {
		const double sim = bestPhraseSimilarity(lastNorm_, k);
		if (sim >= keywordThreshold(k, spec_.fuzzy) && sim > best) {
			best = sim;
			bestWhat = "keyword '" + k + "'";
		}
	}
	std::string matchedPattern;
	// Patterns run on the raw text (keeps punctuation users may rely on) and on
	// the normalized text.
	if (anyPatternMatches(patterns_, lastRaw_, &matchedPattern) ||
	    anyPatternMatches(patterns_, lastNorm_, &matchedPattern)) {
		best = 1.0;
		bestWhat = "pattern /" + matchedPattern + "/";
	}
	out.score = best;
	out.active = best > 0.0;
	out.detail = out.active ? bestWhat : "text did not match";
	if (out.veto)
		out.detail += " (veto: " + out.vetoReason + ")";
}

void OcrDetector::process(const DetectorContext &ctx, DetectorOutput &out)
{
	out.available = available();
	if (!out.available) {
		out.detail = unavailableReason();
		return;
	}
	const auto t0 = std::chrono::steady_clock::now();
	const bool changed = !haveResult_ || hammingDistance(ctx.regionHash, lastHash_) > 2;
	const bool stale = spec_.refreshMs > 0 && ctx.timeMs - lastRunMs_ >= spec_.refreshMs;
	const bool allowed = ctx.timeMs - lastRunMs_ >= ctx.ocrMinIntervalMs;
	if ((changed || stale) && allowed) {
		// OCR engines only need luminance; upscaling gray is 4x cheaper than RGBA.
		const Image &src = *ctx.gray;
		const Image *input = &src;
		if (spec_.upscale > 1.01) {
			const int w = static_cast<int>(std::lround(src.width() * spec_.upscale));
			const int h = static_cast<int>(std::lround(src.height() * spec_.upscale));
			if (static_cast<int64_t>(w) * h <= 4000000) {
				resize(src, w, h, scratch_);
				input = &scratch_;
			}
		}
		const OcrResult r = engine_->recognize(*input);
		lastRunMs_ = ctx.timeMs;
		lastHash_ = ctx.regionHash;
		haveResult_ = true;
		if (r.ok) {
			lastRaw_ = r.text;
			lastNorm_ = normalizeText(r.text);
			lastBoxes_.clear();
			const double inv = input == &scratch_ ? 1.0 / spec_.upscale : 1.0;
			for (const auto &line : r.lines)
				for (const auto &w : line.words)
					lastBoxes_.push_back(PixelRect{static_cast<int>(w.box.x * inv),
								       static_cast<int>(w.box.y * inv),
								       static_cast<int>(std::ceil(w.box.w * inv)),
								       static_cast<int>(std::ceil(w.box.h * inv))});
		} else {
			KS_LOG_DEBUG("OCR '%s' failed: %s", spec_.id.c_str(), r.error.c_str());
			lastRaw_.clear();
			lastNorm_.clear();
			lastBoxes_.clear();
		}
		out.ran = true;
	}
	evaluate(out);
	out.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

} // namespace killsense::detail
