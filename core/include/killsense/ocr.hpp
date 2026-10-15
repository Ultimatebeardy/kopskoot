// KopSkoot - OCR engine abstraction.
// Engines are created and used on the analysis worker thread only.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "killsense/geometry.hpp"
#include "killsense/image.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace killsense {

struct OcrWord {
	std::string text;
	PixelRect box; // in the coordinates of the image passed to recognize()
};

struct OcrLine {
	std::string text;
	std::vector<OcrWord> words;
};

struct OcrResult {
	bool ok = false;
	std::string text; // all lines joined by '\n'
	std::vector<OcrLine> lines;
	std::string error;
};

class IOcrEngine {
public:
	virtual ~IOcrEngine() = default;
	virtual const char *name() const = 0;
	virtual bool available() const = 0;
	// Accepts Gray8 or RGBA8. Must not throw.
	virtual OcrResult recognize(const Image &image) = 0;
};

// Always unavailable; used where no OCR backend exists.
class NullOcrEngine final : public IOcrEngine {
public:
	const char *name() const override { return "none"; }
	bool available() const override { return false; }
	OcrResult recognize(const Image &) override
	{
		OcrResult r;
		r.error = "no OCR engine available";
		return r;
	}
};

// Test/replay engine: returns scripted text supplied by a callback.
class ScriptedOcrEngine final : public IOcrEngine {
public:
	using Provider = std::function<std::string(const Image &)>;
	explicit ScriptedOcrEngine(Provider provider) : provider_(std::move(provider)) {}
	const char *name() const override { return "scripted"; }
	bool available() const override { return true; }
	OcrResult recognize(const Image &image) override;

private:
	Provider provider_;
};

// Creates the best local OCR backend for this platform:
//  - Windows 10/11: Windows.Media.Ocr (built into the OS, fully local)
//  - elsewhere / when disabled at build time: NullOcrEngine
// languageTag: BCP-47 tag such as "en-US"; empty = user profile languages.
std::unique_ptr<IOcrEngine> createPlatformOcrEngine(const std::string &languageTag);

// Per-thread initialisation required by some backends (COM/WinRT apartment).
// Call once at the start of the worker thread and the matching deinit at exit.
void ocrThreadInit();
void ocrThreadDeinit();

} // namespace killsense
