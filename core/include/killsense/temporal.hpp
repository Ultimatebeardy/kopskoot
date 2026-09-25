// REALM KillSense - temporal confirmation and duplicate suppression.
//
// A kill message usually stays on screen for seconds. The tracker turns a
// stream of per-frame fusion results into discrete events:
//
//   Idle --(passes for confirmFrames)--> Active   => emit (unless duplicate)
//   Active --(fails for releaseFrames)--> Idle
//
// While Active, NEW kills are still recognised (rapid consecutive kills):
//   * instance count rises (e.g. a second kill-feed icon)   => emit delta
//   * OCR text changes to a different message (targets without instance
//     counting only)                                        => emit 1
// Fingerprints (dHash + normalized OCR text) of recent events suppress a
// message that disappears briefly and re-appears within cooldownMs.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "killsense/profile.hpp"

#include <cstdint>
#include <deque>
#include <string>

namespace killsense {

struct Observation {
	int64_t timeMs = 0;
	double confidence = 0.0;
	bool passes = false;
	int instanceCount = 0;
	uint64_t visualHash = 0;
	std::string text; // normalized OCR text ("" when unavailable)
};

enum class TrackState { Idle, Active };

struct TemporalDecision {
	int emitCount = 0; // number of events to emit for this frame
	std::string reason;
	bool suppressed = false;
	std::string suppressReason;
	TrackState state = TrackState::Idle;
};

class TemporalTracker {
public:
	TemporalTracker(const TemporalSpec &spec, double threshold, bool useInstances);
	TemporalDecision update(const Observation &obs);
	void reset();
	TrackState state() const { return state_; }

private:
	struct Fingerprint {
		uint64_t id = 0;
		uint64_t hash = 0;
		std::string text;
		int64_t lastSeenMs = 0;
	};
	bool isRecentDuplicate(const Observation &obs, int64_t *ageMs) const;
	uint64_t remember(const Observation &obs);
	void touch(uint64_t id, const Observation &obs);
	bool debounced(int64_t now) const;

	TemporalSpec spec_;
	double threshold_;
	bool useInstances_;

	TrackState state_ = TrackState::Idle;
	int passStreak_ = 0;
	int failStreak_ = 0;
	int64_t lastEventMs_ = 0;
	bool haveEvent_ = false;

	int episodeMaxInstances_ = 0;
	int higherFrames_ = 0;
	int lowerFrames_ = 0;
	std::string episodeText_;
	std::string pendingText_;
	int newTextFrames_ = 0;
	uint64_t episodeFp_ = 0;

	std::deque<Fingerprint> recent_;
	uint64_t nextFpId_ = 1;
};

} // namespace killsense
