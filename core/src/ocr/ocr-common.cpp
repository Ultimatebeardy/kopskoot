// SPDX-License-Identifier: GPL-2.0-or-later
#include "killsense/ocr.hpp"

#include "killsense/logging.hpp"

namespace killsense {

OcrResult ScriptedOcrEngine::recognize(const Image &image)
{
	OcrResult r;
	try {
		r.text = provider_ ? provider_(image) : std::string();
		r.ok = true;
		OcrLine line;
		line.text = r.text;
		r.lines.push_back(std::move(line));
	} catch (const std::exception &e) {
		r.error = e.what();
	}
	return r;
}

#if !defined(KILLSENSE_HAS_WINDOWS_OCR)
std::unique_ptr<IOcrEngine> createPlatformOcrEngine(const std::string &)
{
	KS_LOG_INFO("no platform OCR backend in this build; OCR detectors will report unavailable");
	return std::make_unique<NullOcrEngine>();
}

void ocrThreadInit() {}
void ocrThreadDeinit() {}
#endif

} // namespace killsense
