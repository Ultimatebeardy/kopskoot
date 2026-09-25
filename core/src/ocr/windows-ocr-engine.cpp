// REALM KillSense - Windows.Media.Ocr backend (C++/WinRT).
// Uses the OCR engine built into Windows 10/11. Everything runs locally; no
// images leave the machine. Compiled only when KILLSENSE_HAS_WINDOWS_OCR=1.
// SPDX-License-Identifier: GPL-2.0-or-later
#if defined(KILLSENSE_HAS_WINDOWS_OCR)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <unknwn.h>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.Storage.Streams.h>

#include "killsense/logging.hpp"
#include "killsense/ocr.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace killsense {

namespace wf = winrt::Windows::Foundation;
namespace wgi = winrt::Windows::Graphics::Imaging;
namespace wmo = winrt::Windows::Media::Ocr;
namespace wsc = winrt::Windows::Security::Cryptography;

namespace {

thread_local bool t_apartmentInitialized = false;

class WindowsOcrEngine final : public IOcrEngine {
public:
	explicit WindowsOcrEngine(const std::string &languageTag)
	{
		try {
			if (!languageTag.empty()) {
				winrt::Windows::Globalization::Language lang{winrt::to_hstring(languageTag)};
				if (wmo::OcrEngine::IsLanguageSupported(lang))
					engine_ = wmo::OcrEngine::TryCreateFromLanguage(lang);
				else
					KS_LOG_WARN(
						"Windows OCR: language '%s' is not installed, using profile languages",
						languageTag.c_str());
			}
			if (!engine_)
				engine_ = wmo::OcrEngine::TryCreateFromUserProfileLanguages();
			if (engine_) {
				maxDim_ = static_cast<int>(wmo::OcrEngine::MaxImageDimension());
				KS_LOG_INFO("Windows OCR ready (language: %s)",
					    winrt::to_string(engine_.RecognizerLanguage().LanguageTag()).c_str());
			} else {
				KS_LOG_WARN("Windows OCR unavailable: no OCR language pack installed. Install one via "
					    "Settings > Time & language > Language & region.");
			}
		} catch (const winrt::hresult_error &e) {
			KS_LOG_WARN("Windows OCR init failed: %s", winrt::to_string(e.message()).c_str());
			engine_ = nullptr;
		} catch (...) {
			KS_LOG_WARN("Windows OCR init failed (unknown error)");
			engine_ = nullptr;
		}
	}

	const char *name() const override { return "windows.media.ocr"; }
	bool available() const override { return static_cast<bool>(engine_); }

	OcrResult recognize(const Image &image) override
	{
		OcrResult result;
		if (!engine_) {
			result.error = "Windows OCR not available";
			return result;
		}
		if (image.empty() || image.width() > maxDim_ || image.height() > maxDim_ || image.width() < 8 ||
		    image.height() < 8) {
			result.error = "image size not supported by Windows OCR";
			return result;
		}
		try {
			// Convert to BGRA8 as required by SoftwareBitmap.
			const size_t n = static_cast<size_t>(image.width()) * image.height();
			bgra_.resize(n * 4);
			const uint8_t *src = image.data();
			if (image.format() == PixelFormat::RGBA8) {
				for (size_t i = 0; i < n; ++i) {
					bgra_[i * 4 + 0] = src[i * 4 + 2];
					bgra_[i * 4 + 1] = src[i * 4 + 1];
					bgra_[i * 4 + 2] = src[i * 4 + 0];
					bgra_[i * 4 + 3] = 255;
				}
			} else {
				for (size_t i = 0; i < n; ++i) {
					bgra_[i * 4 + 0] = bgra_[i * 4 + 1] = bgra_[i * 4 + 2] = src[i];
					bgra_[i * 4 + 3] = 255;
				}
			}
			const auto buffer = wsc::CryptographicBuffer::CreateFromByteArray(
				winrt::array_view<uint8_t const>(bgra_.data(), bgra_.data() + bgra_.size()));
			const auto bitmap = wgi::SoftwareBitmap::CreateCopyFromBuffer(
				buffer, wgi::BitmapPixelFormat::Bgra8, image.width(), image.height());

			// Blocking wait is valid: the worker thread is an MTA thread.
			const wmo::OcrResult ocr = engine_.RecognizeAsync(bitmap).get();
			for (const auto &line : ocr.Lines()) {
				OcrLine l;
				l.text = winrt::to_string(line.Text());
				for (const auto &word : line.Words()) {
					OcrWord w;
					w.text = winrt::to_string(word.Text());
					const wf::Rect r = word.BoundingRect();
					w.box = PixelRect{static_cast<int>(r.X), static_cast<int>(r.Y),
							  static_cast<int>(std::ceil(r.Width)),
							  static_cast<int>(std::ceil(r.Height))};
					l.words.push_back(std::move(w));
				}
				if (!result.text.empty())
					result.text += '\n';
				result.text += l.text;
				result.lines.push_back(std::move(l));
			}
			result.ok = true;
		} catch (const winrt::hresult_error &e) {
			result.error = winrt::to_string(e.message());
		} catch (const std::exception &e) {
			result.error = e.what();
		} catch (...) {
			result.error = "unknown Windows OCR error";
		}
		return result;
	}

private:
	wmo::OcrEngine engine_{nullptr};
	int maxDim_ = 4096;
	std::vector<uint8_t> bgra_;
};

} // namespace

std::unique_ptr<IOcrEngine> createPlatformOcrEngine(const std::string &languageTag)
{
	try {
		auto engine = std::make_unique<WindowsOcrEngine>(languageTag);
		if (engine->available())
			return engine;
	} catch (...) {
	}
	return std::make_unique<NullOcrEngine>();
}

void ocrThreadInit()
{
	try {
		winrt::init_apartment(winrt::apartment_type::multi_threaded);
		t_apartmentInitialized = true;
	} catch (const winrt::hresult_error &e) {
		// RPC_E_CHANGED_MODE: thread already has an apartment; keep going.
		KS_LOG_DEBUG("WinRT apartment init: %s", winrt::to_string(e.message()).c_str());
		t_apartmentInitialized = false;
	} catch (...) {
		t_apartmentInitialized = false;
	}
}

void ocrThreadDeinit()
{
	if (t_apartmentInitialized) {
		winrt::uninit_apartment();
		t_apartmentInitialized = false;
	}
}

} // namespace killsense

#endif // KILLSENSE_HAS_WINDOWS_OCR
