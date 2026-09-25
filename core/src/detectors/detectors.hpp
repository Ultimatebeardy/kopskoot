// REALM KillSense - built-in detector implementations (internal header).
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "killsense/detector.hpp"
#include "killsense/text-match.hpp"

#include <map>

namespace killsense::detail {

class TemplateDetector final : public IDetector {
public:
	explicit TemplateDetector(const DetectorSpec &spec);
	const DetectorSpec &spec() const override { return spec_; }
	bool available() const override { return loaded_; }
	std::string unavailableReason() const override { return error_; }
	void process(const DetectorContext &ctx, DetectorOutput &out) override;

private:
	TemplateMatcher &matcherFor(double effectiveScale);

	DetectorSpec spec_;
	Image original_;
	bool loaded_ = false;
	std::string error_;
	std::map<int, TemplateMatcher> matchers_; // key: scale * 100
};

class OcrDetector final : public IDetector {
public:
	OcrDetector(const DetectorSpec &spec, IOcrEngine *engine);
	const DetectorSpec &spec() const override { return spec_; }
	bool available() const override { return engine_ && engine_->available(); }
	std::string unavailableReason() const override;
	void process(const DetectorContext &ctx, DetectorOutput &out) override;
	void reset() override;

private:
	void evaluate(DetectorOutput &out) const;

	DetectorSpec spec_;
	IOcrEngine *engine_ = nullptr;
	CompiledPatterns patterns_;
	std::vector<std::string> keywordsNorm_;
	std::vector<std::string> negativeNorm_;
	Image scratch_;
	// Cached OCR result: OCR only re-runs when the region changes.
	bool haveResult_ = false;
	uint64_t lastHash_ = 0;
	int64_t lastRunMs_ = -1000000;
	std::string lastRaw_;
	std::string lastNorm_;
	std::vector<PixelRect> lastBoxes_;
	std::vector<OcrLine> lastLines_; // word boxes in region coordinates (for kill-feed roles)
};

class ColorDetector final : public IDetector {
public:
	explicit ColorDetector(const DetectorSpec &spec) : spec_(spec) {}
	const DetectorSpec &spec() const override { return spec_; }
	bool available() const override { return true; }
	void process(const DetectorContext &ctx, DetectorOutput &out) override;

private:
	DetectorSpec spec_;
};

class HudChangeDetector final : public IDetector {
public:
	explicit HudChangeDetector(const DetectorSpec &spec) : spec_(spec) {}
	const DetectorSpec &spec() const override { return spec_; }
	bool available() const override { return true; }
	void process(const DetectorContext &ctx, DetectorOutput &out) override;
	void reset() override;

private:
	DetectorSpec spec_;
	Image prev_;
	Image cur_;
	bool havePrev_ = false;
	int64_t lastChangeMs_ = -1000000;
};

// Placeholder for kinds that are declared in the schema but not yet built
// (e.g. ONNX in Milestone 4). Always reports unavailable - never fakes evidence.
class UnavailableDetector final : public IDetector {
public:
	UnavailableDetector(const DetectorSpec &spec, std::string reason) : spec_(spec), reason_(std::move(reason)) {}
	const DetectorSpec &spec() const override { return spec_; }
	bool available() const override { return false; }
	std::string unavailableReason() const override { return reason_; }
	void process(const DetectorContext &, DetectorOutput &out) override
	{
		out.available = false;
		out.detail = reason_;
	}

private:
	DetectorSpec spec_;
	std::string reason_;
};

} // namespace killsense::detail
