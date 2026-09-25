// SPDX-License-Identifier: GPL-2.0-or-later
#include "killsense/calibration.hpp"

#include <algorithm>
#include <cmath>

namespace killsense {

void CalibrationRecorder::addFrame(int64_t timeMs, const Image &gray)
{
	if (gray.empty() || gray.format() != PixelFormat::Gray8)
		return;
	if (!buffer_.empty() &&
	    (buffer_.back().gray.width() != gray.width() || buffer_.back().gray.height() != gray.height()))
		buffer_.clear(); // resolution changed; old frames are not comparable
	CalibrationFrame f;
	f.timeMs = timeMs;
	f.gray = gray;
	if (pending_) {
		current_.after.push_back(f);
		if (timeMs - current_.markTimeMs >= settings_.postMs)
			finishPending();
	}
	buffer_.push_back(std::move(f));
	while (!buffer_.empty() &&
	       (timeMs - buffer_.front().timeMs > settings_.bufferMs || buffer_.size() > settings_.maxBufferedFrames))
		buffer_.pop_front();
}

bool CalibrationRecorder::mark(int64_t timeMs)
{
	if (pending_ || samples_.size() >= settings_.maxSamples)
		return false;
	current_ = CalibrationSample{};
	current_.markTimeMs = timeMs;
	for (const auto &f : buffer_)
		if (f.timeMs < timeMs && timeMs - f.timeMs <= settings_.preMs)
			current_.before.push_back(f);
	pending_ = true;
	return true;
}

void CalibrationRecorder::finishPending()
{
	pending_ = false;
	if (!current_.before.empty() && !current_.after.empty())
		samples_.push_back(std::move(current_));
	current_ = CalibrationSample{};
}

void CalibrationRecorder::removeSample(size_t index)
{
	if (index < samples_.size())
		samples_.erase(samples_.begin() + static_cast<std::ptrdiff_t>(index));
}

void CalibrationRecorder::clear()
{
	buffer_.clear();
	samples_.clear();
	pending_ = false;
	current_ = CalibrationSample{};
}

namespace {

// Per-cell mean absolute difference between two same-size gray images.
std::vector<double> cellDiff(const Image &a, const Image &b, int cols, int rows)
{
	std::vector<double> sum(static_cast<size_t>(cols) * rows, 0.0);
	std::vector<int> cnt(sum.size(), 0);
	for (int y = 0; y < a.height(); ++y) {
		const int cy = std::min(rows - 1, y * rows / a.height());
		const uint8_t *ra = a.row(y);
		const uint8_t *rb = b.row(y);
		for (int x = 0; x < a.width(); ++x) {
			const int cx = std::min(cols - 1, x * cols / a.width());
			const size_t i = static_cast<size_t>(cy) * cols + cx;
			sum[i] += std::abs(static_cast<int>(ra[x]) - static_cast<int>(rb[x]));
			++cnt[i];
		}
	}
	for (size_t i = 0; i < sum.size(); ++i)
		sum[i] = cnt[i] ? sum[i] / cnt[i] : 0.0;
	return sum;
}

} // namespace

std::vector<CandidateRegion> analyzeCandidates(const std::vector<CalibrationSample> &samples, int cols, int rows,
					       size_t maxCandidates, double minConsistency)
{
	std::vector<CandidateRegion> out;
	if (samples.empty() || cols <= 0 || rows <= 0)
		return out;
	const size_t n = static_cast<size_t>(cols) * rows;
	std::vector<int> hits(n, 0);
	std::vector<double> strength(n, 0.0);
	int usable = 0;

	for (const auto &s : samples) {
		if (s.before.empty() || s.after.empty())
			continue;
		const Image &ref = s.before.back().gray; // last frame before the mark
		if (ref.empty())
			continue;
		// Baseline: typical change between consecutive "before" frames (camera motion etc.).
		std::vector<double> baseline(n, 0.0);
		int baseCount = 0;
		for (size_t i = 1; i < s.before.size(); ++i) {
			if (s.before[i].gray.width() != ref.width() || s.before[i].gray.height() != ref.height())
				continue;
			const auto d = cellDiff(s.before[i - 1].gray, s.before[i].gray, cols, rows);
			for (size_t k = 0; k < n; ++k)
				baseline[k] = std::max(baseline[k], d[k]);
			++baseCount;
		}
		// Peak change after the mark relative to the last "before" frame.
		std::vector<double> peak(n, 0.0);
		for (const auto &f : s.after) {
			if (f.gray.width() != ref.width() || f.gray.height() != ref.height())
				continue;
			const auto d = cellDiff(ref, f.gray, cols, rows);
			for (size_t k = 0; k < n; ++k)
				peak[k] = std::max(peak[k], d[k]);
		}
		++usable;
		for (size_t k = 0; k < n; ++k) {
			const double margin = baseCount ? baseline[k] * 1.5 + 4.0 : 8.0;
			if (peak[k] > margin) {
				++hits[k];
				strength[k] += peak[k] - margin;
			}
		}
	}
	if (usable == 0)
		return out;

	// Connected components of consistent cells.
	std::vector<char> on(n, 0), seen(n, 0);
	for (size_t k = 0; k < n; ++k)
		on[k] = static_cast<double>(hits[k]) / usable >= minConsistency;
	for (int y = 0; y < rows; ++y) {
		for (int x = 0; x < cols; ++x) {
			const size_t start = static_cast<size_t>(y) * cols + x;
			if (!on[start] || seen[start])
				continue;
			int x0 = x, x1 = x, y0 = y, y1 = y;
			double cons = 0.0, str = 0.0;
			int cells = 0;
			std::vector<size_t> stack{start};
			seen[start] = 1;
			while (!stack.empty()) {
				const size_t k = stack.back();
				stack.pop_back();
				const int cx = static_cast<int>(k % cols), cy = static_cast<int>(k / cols);
				x0 = std::min(x0, cx);
				x1 = std::max(x1, cx);
				y0 = std::min(y0, cy);
				y1 = std::max(y1, cy);
				cons += static_cast<double>(hits[k]) / usable;
				str += hits[k] ? strength[k] / hits[k] : 0.0;
				++cells;
				const int nx[4] = {cx - 1, cx + 1, cx, cx};
				const int ny[4] = {cy, cy, cy - 1, cy + 1};
				for (int i = 0; i < 4; ++i) {
					if (nx[i] < 0 || ny[i] < 0 || nx[i] >= cols || ny[i] >= rows)
						continue;
					const size_t kk = static_cast<size_t>(ny[i]) * cols + nx[i];
					if (on[kk] && !seen[kk]) {
						seen[kk] = 1;
						stack.push_back(kk);
					}
				}
			}
			CandidateRegion c;
			// One cell of padding on each side, clamped to the frame.
			const int px0 = std::max(0, x0 - 1), py0 = std::max(0, y0 - 1);
			const int px1 = std::min(cols - 1, x1 + 1), py1 = std::min(rows - 1, y1 + 1);
			c.rect = NormRect{static_cast<double>(px0) / cols, static_cast<double>(py0) / rows,
					  static_cast<double>(px1 - px0 + 1) / cols,
					  static_cast<double>(py1 - py0 + 1) / rows};
			c.consistency = cons / cells;
			c.strength = str / cells;
			out.push_back(c);
		}
	}
	// Prefer consistent, strong and compact regions (HUD elements are small).
	std::sort(out.begin(), out.end(), [](const CandidateRegion &a, const CandidateRegion &b) {
		const double sa = a.consistency * a.strength / std::sqrt(a.rect.w * a.rect.h + 1e-3);
		const double sb = b.consistency * b.strength / std::sqrt(b.rect.w * b.rect.h + 1e-3);
		return sa > sb;
	});
	if (out.size() > maxCandidates)
		out.resize(maxCandidates);
	return out;
}

} // namespace killsense
