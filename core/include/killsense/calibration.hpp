// REALM KillSense - "Learn This Game" calibration core.
//
// Status: core logic implemented and unit-tested (Milestone 1).
// OBS integration (full-frame low-res sampling, hotkey, candidate review UI,
// profile generation) is PLANNED for Milestone 2 - see docs/MILESTONES.md.
//
// Workflow: keep a rolling buffer of small grayscale frames; when the user
// presses the "mark kill" hotkey, store frames from before and after the mark;
// after several marks, find screen cells that consistently change right after
// the marks and propose them as candidate regions of interest.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "killsense/geometry.hpp"
#include "killsense/image.hpp"

#include <cstdint>
#include <deque>
#include <vector>

namespace killsense {

struct CalibrationFrame {
	int64_t timeMs = 0;
	Image gray; // small full-frame gray image (e.g. 320x180)
};

struct CalibrationSample {
	int64_t markTimeMs = 0;
	std::vector<CalibrationFrame> before;
	std::vector<CalibrationFrame> after;
};

struct CandidateRegion {
	NormRect rect;
	double consistency = 0.0; // fraction of samples in which the area changed
	double strength = 0.0;    // mean change magnitude above baseline
};

struct CalibrationSettings {
	int bufferMs = 5000;
	int preMs = 1500;
	int postMs = 1500;
	size_t maxSamples = 20;
	size_t maxBufferedFrames = 120;
};

class CalibrationRecorder {
public:
	explicit CalibrationRecorder(CalibrationSettings settings = {}) : settings_(settings) {}

	// Frames must be the same size and arrive in time order.
	void addFrame(int64_t timeMs, const Image &gray);
	// Marks a kill at timeMs. Returns false if another mark is still collecting
	// "after" frames or the sample limit is reached.
	bool mark(int64_t timeMs);
	bool collecting() const { return pending_; }
	const std::vector<CalibrationSample> &samples() const { return samples_; }
	void removeSample(size_t index);
	void clear();

private:
	void finishPending();

	CalibrationSettings settings_;
	std::deque<CalibrationFrame> buffer_;
	std::vector<CalibrationSample> samples_;
	bool pending_ = false;
	CalibrationSample current_;
};

// Finds regions that consistently change after the marked events.
std::vector<CandidateRegion> analyzeCandidates(const std::vector<CalibrationSample> &samples, int gridCols = 32,
					       int gridRows = 18, size_t maxCandidates = 3,
					       double minConsistency = 0.6);

} // namespace killsense
