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
		// One OcrLine per text line; words get synthetic boxes (12 px per
		// character, 30 px per line) so geometry-based logic can be tested.
		int y = 0;
		size_t start = 0;
		while (start <= r.text.size()) {
			size_t end = r.text.find('\n', start);
			if (end == std::string::npos)
				end = r.text.size();
			OcrLine line;
			line.text = r.text.substr(start, end - start);
			if (!line.text.empty() && line.text.back() == '\r')
				line.text.pop_back();
			int x = 0;
			size_t p = 0;
			while (p < line.text.size()) {
				while (p < line.text.size() && line.text[p] == ' ')
					++p, x += 12;
				size_t q = p;
				while (q < line.text.size() && line.text[q] != ' ')
					++q;
				if (q > p) {
					const int w = static_cast<int>(q - p) * 12;
					line.words.push_back(OcrWord{line.text.substr(p, q - p), PixelRect{x, y, w, 20}});
					x += w;
				}
				p = q;
			}
			if (!line.text.empty())
				r.lines.push_back(std::move(line));
			y += 30;
			start = end + 1;
		}
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
