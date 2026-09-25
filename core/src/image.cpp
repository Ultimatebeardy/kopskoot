// SPDX-License-Identifier: GPL-2.0-or-later
#include "killsense/image.hpp"
#include "killsense/fsutil.hpp"

#include <algorithm>
#include <bitset>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#define STBI_WRITE_NO_STDIO
#include "stb_image_write.h"
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

namespace killsense {

void copyRgbaFromStrided(const uint8_t *src, uint32_t srcLinesize, int width, int height, Image &out)
{
	out.reset(width, height, PixelFormat::RGBA8);
	if (!src || width <= 0 || height <= 0)
		return;
	const size_t rowBytes = static_cast<size_t>(width) * 4;
	for (int y = 0; y < height; ++y)
		std::memcpy(out.row(y), src + static_cast<size_t>(y) * srcLinesize, rowBytes);
}

void toGray(const Image &src, Image &out)
{
	out.reset(src.width(), src.height(), PixelFormat::Gray8);
	if (src.empty())
		return;
	if (src.format() == PixelFormat::Gray8) {
		std::memcpy(out.data(), src.data(), src.byteSize());
		return;
	}
	const size_t n = static_cast<size_t>(src.width()) * src.height();
	const uint8_t *s = src.data();
	uint8_t *d = out.data();
	for (size_t i = 0; i < n; ++i, s += 4) {
		// Integer BT.601: (77 R + 150 G + 29 B) >> 8
		d[i] = static_cast<uint8_t>((77u * s[0] + 150u * s[1] + 29u * s[2]) >> 8);
	}
}

void crop(const Image &src, const PixelRect &rect, Image &out)
{
	const PixelRect r = intersect(rect, PixelRect{0, 0, src.width(), src.height()});
	out.reset(r.w, r.h, src.format());
	if (r.empty())
		return;
	const size_t rowBytes = static_cast<size_t>(r.w) * src.channels();
	for (int y = 0; y < r.h; ++y)
		std::memcpy(out.row(y), src.row(r.y + y) + static_cast<size_t>(r.x) * src.channels(), rowBytes);
}

namespace {

void resizeBox(const Image &src, int width, int height, Image &out)
{
	const int ch = src.channels();
	const double sx = static_cast<double>(src.width()) / width;
	const double sy = static_cast<double>(src.height()) / height;
	std::vector<uint32_t> acc(static_cast<size_t>(ch));
	for (int y = 0; y < height; ++y) {
		const int y0 = static_cast<int>(y * sy);
		const int y1 = std::max(y0 + 1, std::min(src.height(), static_cast<int>((y + 1) * sy)));
		uint8_t *drow = out.row(y);
		for (int x = 0; x < width; ++x) {
			const int x0 = static_cast<int>(x * sx);
			const int x1 = std::max(x0 + 1, std::min(src.width(), static_cast<int>((x + 1) * sx)));
			std::fill(acc.begin(), acc.end(), 0u);
			for (int yy = y0; yy < y1; ++yy) {
				const uint8_t *s = src.row(yy) + static_cast<size_t>(x0) * ch;
				for (int xx = x0; xx < x1; ++xx)
					for (int c = 0; c < ch; ++c)
						acc[static_cast<size_t>(c)] += *s++;
			}
			const uint32_t count = static_cast<uint32_t>((y1 - y0) * (x1 - x0));
			for (int c = 0; c < ch; ++c)
				drow[x * ch + c] =
					static_cast<uint8_t>((acc[static_cast<size_t>(c)] + count / 2) / count);
		}
	}
}

void resizeBilinear(const Image &src, int width, int height, Image &out)
{
	// Fixed-point (8-bit weights) bilinear interpolation with precomputed
	// column tables: this runs every frame for OCR upscaling.
	const int ch = src.channels();
	const double sx = static_cast<double>(src.width()) / width;
	const double sy = static_cast<double>(src.height()) / height;
	std::vector<int> xo0(static_cast<size_t>(width)), xo1(static_cast<size_t>(width)),
		xw(static_cast<size_t>(width));
	for (int x = 0; x < width; ++x) {
		double fx = std::clamp((x + 0.5) * sx - 0.5, 0.0, static_cast<double>(src.width() - 1));
		const int x0 = static_cast<int>(fx);
		xo0[static_cast<size_t>(x)] = x0 * ch;
		xo1[static_cast<size_t>(x)] = std::min(x0 + 1, src.width() - 1) * ch;
		xw[static_cast<size_t>(x)] = static_cast<int>(std::lround((fx - x0) * 256.0));
	}
	for (int y = 0; y < height; ++y) {
		const double fy = std::clamp((y + 0.5) * sy - 0.5, 0.0, static_cast<double>(src.height() - 1));
		const int y0 = static_cast<int>(fy);
		const int y1 = std::min(y0 + 1, src.height() - 1);
		const int wy = static_cast<int>(std::lround((fy - y0) * 256.0));
		uint8_t *drow = out.row(y);
		const uint8_t *r0 = src.row(y0);
		const uint8_t *r1 = src.row(y1);
		for (int x = 0; x < width; ++x) {
			const int a0 = xo0[static_cast<size_t>(x)], a1 = xo1[static_cast<size_t>(x)];
			const int wx = xw[static_cast<size_t>(x)];
			for (int c = 0; c < ch; ++c) {
				const int top = r0[a0 + c] * (256 - wx) + r0[a1 + c] * wx;
				const int bot = r1[a0 + c] * (256 - wx) + r1[a1 + c] * wx;
				drow[x * ch + c] =
					static_cast<uint8_t>((top * (256 - wy) + bot * wy + (1 << 15)) >> 16);
			}
		}
	}
}

} // namespace

void resize(const Image &src, int width, int height, Image &out)
{
	if (src.empty() || width <= 0 || height <= 0) {
		out.reset(0, 0, src.format());
		return;
	}
	if (&src == &out)
		return; // In-place resizing is not supported.
	out.reset(width, height, src.format());
	if (width == src.width() && height == src.height()) {
		std::memcpy(out.data(), src.data(), src.byteSize());
		return;
	}
	if (src.width() >= width * 2 || src.height() >= height * 2)
		resizeBox(src, width, height, out);
	else
		resizeBilinear(src, width, height, out);
}

uint64_t dHash64(const Image &src)
{
	if (src.empty())
		return 0;
	Image gray;
	const Image *g = &src;
	if (src.format() != PixelFormat::Gray8) {
		toGray(src, gray);
		g = &gray;
	}
	Image small;
	resize(*g, 9, 8, small);
	uint64_t h = 0;
	int bit = 0;
	for (int y = 0; y < 8; ++y) {
		const uint8_t *r = small.row(y);
		for (int x = 0; x < 8; ++x, ++bit)
			if (r[x] > r[x + 1])
				h |= (uint64_t{1} << bit);
	}
	return h;
}

int hammingDistance(uint64_t a, uint64_t b)
{
	return static_cast<int>(std::bitset<64>(a ^ b).count());
}

double meanAbsDiff(const Image &a, const Image &b)
{
	if (a.empty() || a.width() != b.width() || a.height() != b.height() || a.format() != b.format())
		return 255.0;
	const size_t n = a.byteSize();
	uint64_t sum = 0;
	const uint8_t *pa = a.data();
	const uint8_t *pb = b.data();
	for (size_t i = 0; i < n; ++i)
		sum += static_cast<uint64_t>(std::abs(static_cast<int>(pa[i]) - static_cast<int>(pb[i])));
	return static_cast<double>(sum) / static_cast<double>(n);
}

bool loadImageFile(const std::string &path, Image &out, PixelFormat fmt)
{
	// Read the file ourselves so UTF-8 paths work on Windows.
	std::ifstream in(pathFromUtf8(path), std::ios::binary);
	if (!in)
		return false;
	std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if (bytes.empty() || bytes.size() > 64u * 1024u * 1024u)
		return false;
	int w = 0, h = 0, comp = 0;
	const int want = fmt == PixelFormat::Gray8 ? 1 : 4;
	unsigned char *pixels =
		stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &comp, want);
	if (!pixels)
		return false;
	out.reset(w, h, fmt);
	std::memcpy(out.data(), pixels, out.byteSize());
	stbi_image_free(pixels);
	return true;
}

namespace {
void writeToVector(void *context, void *data, int size)
{
	auto *vec = static_cast<std::vector<unsigned char> *>(context);
	const auto *bytes = static_cast<const unsigned char *>(data);
	vec->insert(vec->end(), bytes, bytes + size);
}
} // namespace

bool savePng(const std::string &path, const Image &img)
{
	if (img.empty())
		return false;
	std::vector<unsigned char> encoded;
	if (!stbi_write_png_to_func(writeToVector, &encoded, img.width(), img.height(), img.channels(), img.data(),
				    img.stride()))
		return false;
	std::ofstream outFile(pathFromUtf8(path), std::ios::binary | std::ios::trunc);
	if (!outFile)
		return false;
	outFile.write(reinterpret_cast<const char *>(encoded.data()), static_cast<std::streamsize>(encoded.size()));
	return static_cast<bool>(outFile);
}

} // namespace killsense
