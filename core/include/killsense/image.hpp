// KopSkoot - simple owning image type and CPU image operations.
// Buffers are reused across frames: every function writes into an output
// image that is only reallocated when its size grows.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "killsense/geometry.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace killsense {

enum class PixelFormat : uint8_t { Gray8 = 1, RGBA8 = 4 };

class Image {
public:
	Image() = default;
	Image(int width, int height, PixelFormat fmt) { reset(width, height, fmt); }

	// Resizes the image. Existing capacity is reused; contents are undefined.
	void reset(int width, int height, PixelFormat fmt)
	{
		width_ = width > 0 ? width : 0;
		height_ = height > 0 ? height : 0;
		format_ = fmt;
		pixels_.resize(static_cast<size_t>(width_) * height_ * channels());
	}
	void clear()
	{
		width_ = height_ = 0;
		pixels_.clear();
	}

	int width() const { return width_; }
	int height() const { return height_; }
	PixelFormat format() const { return format_; }
	int channels() const { return static_cast<int>(format_); }
	int stride() const { return width_ * channels(); }
	bool empty() const { return width_ == 0 || height_ == 0; }
	size_t byteSize() const { return pixels_.size(); }

	uint8_t *data() { return pixels_.data(); }
	const uint8_t *data() const { return pixels_.data(); }
	uint8_t *row(int y) { return pixels_.data() + static_cast<size_t>(y) * stride(); }
	const uint8_t *row(int y) const { return pixels_.data() + static_cast<size_t>(y) * stride(); }

private:
	int width_ = 0;
	int height_ = 0;
	PixelFormat format_ = PixelFormat::RGBA8;
	std::vector<uint8_t> pixels_;
};

// Copies rows from a strided RGBA source (e.g. a mapped OBS stage surface).
void copyRgbaFromStrided(const uint8_t *src, uint32_t srcLinesize, int width, int height, Image &out);

// Luma (BT.601) conversion; Gray8 input is copied.
void toGray(const Image &src, Image &out);

// Crops rect (clamped to src) into out, keeping the pixel format.
void crop(const Image &src, const PixelRect &rect, Image &out);

// Bilinear resize, any format. Downscales by more than 2x use box averaging.
void resize(const Image &src, int width, int height, Image &out);

// 64-bit difference hash of a (gray or RGBA) image. Used for duplicate
// suppression and "did this region change" checks.
uint64_t dHash64(const Image &src);
int hammingDistance(uint64_t a, uint64_t b);

// Mean absolute difference between two same-size gray images (0..255).
double meanAbsDiff(const Image &a, const Image &b);

// File IO (PNG/JPG/BMP/TGA via stb_image). Returns false on failure.
bool loadImageFile(const std::string &path, Image &out, PixelFormat fmt);
bool savePng(const std::string &path, const Image &img);

} // namespace killsense
