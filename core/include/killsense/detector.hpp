// REALM KillSense - detector module interface.
// Every detector turns one captured region into a single evidence signal.
// New detector kinds are added by implementing IDetector and registering the
// kind in createDetector() (core/src/detectors/detector-factory.cpp).
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "killsense/capture.hpp"
#include "killsense/image.hpp"
#include "killsense/ocr.hpp"
#include "killsense/profile.hpp"

#include <memory>
#include <string>
#include <vector>

namespace killsense {

struct DetectorOutput {
	std::string detectorId;
	DetectorKind kind = DetectorKind::Template;
	std::string regionId;
	bool available = true; // backend present and configuration valid
	bool ran = false;      // produced fresh results this frame (OCR may reuse)
	double score = 0.0;    // 0..1 evidence strength
	bool active = false;   // signal counts toward minSignals
	bool veto = false;     // e.g. OCR negative word matched
	std::string vetoReason;
	int instances = 0;            // distinct matches (template) for rapid kills
	std::vector<PixelRect> boxes; // in region image coordinates
	std::string text;             // normalized OCR text (OCR only)
	std::string rawText;          // raw OCR text (OCR only)
	std::string detail;           // short human readable explanation
	double ms = 0.0;              // processing time
};

struct DetectorContext {
	int64_t timeMs = 0;
	const RegionCapture *region = nullptr;
	const Image *gray = nullptr; // gray version of region->rgba
	uint64_t regionHash = 0;     // dHash of the region
	int ocrMinIntervalMs = 0;
};

class IDetector {
public:
	virtual ~IDetector() = default;
	virtual const DetectorSpec &spec() const = 0;
	virtual bool available() const = 0;
	virtual std::string unavailableReason() const { return {}; }
	virtual void process(const DetectorContext &ctx, DetectorOutput &out) = 0;
	virtual void reset() {}
};

struct DetectorDeps {
	IOcrEngine *ocr = nullptr; // owned by the worker thread
};

std::unique_ptr<IDetector> createDetector(const DetectorSpec &spec, const DetectorDeps &deps);

// ---- Template matching primitive (exposed for tests and calibration) ----
struct TemplateMatch {
	PixelRect box;
	double score = 0.0;
};

// Normalized cross-correlation matcher with coarse-to-fine search.
class TemplateMatcher {
public:
	void setTemplate(const Image &templGray);
	bool hasTemplate() const { return !templ_.empty(); }
	int templateWidth() const { return templ_.width(); }
	int templateHeight() const { return templ_.height(); }
	// Returns non-overlapping matches with score >= threshold (best first),
	// and the best score seen anywhere in `bestScore`.
	std::vector<TemplateMatch> match(const Image &gray, double threshold, int maxInstances, double &bestScore);

private:
	Image templ_;
	Image templCoarse_;
	int coarseFactor_ = 1;
	Image coarse_;
};

// Single NCC evaluation at (x, y); exposed for tests.
double nccAt(const Image &gray, const Image &templ, int x, int y);

// RGB (0..255) -> HSV (h in degrees, s/v in 0..1)
void rgbToHsv(uint8_t r, uint8_t g, uint8_t b, double &h, double &s, double &v);

} // namespace killsense
