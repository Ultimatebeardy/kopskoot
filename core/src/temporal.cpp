// SPDX-License-Identifier: GPL-2.0-or-later
#include "killsense/temporal.hpp"

#include "killsense/image.hpp"
#include "killsense/text-match.hpp"

#include <algorithm>
#include <cstdio>

namespace killsense {

TemporalTracker::TemporalTracker(const TemporalSpec &spec, double threshold, bool useInstances)
	: spec_(spec),
	  threshold_(threshold),
	  useInstances_(useInstances)
{
}

void TemporalTracker::reset()
{
	state_ = TrackState::Idle;
	passStreak_ = failStreak_ = 0;
	haveEvent_ = false;
	lastEventMs_ = 0;
	episodeMaxInstances_ = higherFrames_ = lowerFrames_ = newTextFrames_ = 0;
	episodeText_.clear();
	pendingText_.clear();
	episodeFp_ = 0;
	recent_.clear();
}

bool TemporalTracker::isRecentDuplicate(const Observation &obs, int64_t *ageMs) const
{
	for (const auto &fp : recent_) {
		const int64_t age = obs.timeMs - fp.lastSeenMs;
		if (age < 0 || age > spec_.cooldownMs)
			continue;
		bool same = false;
		if (!obs.text.empty() && !fp.text.empty())
			same = sameMessage(obs.text, fp.text, spec_.textSimilarity);
		else
			same = hammingDistance(obs.visualHash, fp.hash) <= spec_.maxHamming;
		if (same) {
			if (ageMs)
				*ageMs = age;
			return true;
		}
	}
	return false;
}

uint64_t TemporalTracker::remember(const Observation &obs)
{
	Fingerprint fp;
	fp.id = nextFpId_++;
	fp.hash = obs.visualHash;
	fp.text = obs.text;
	fp.lastSeenMs = obs.timeMs;
	recent_.push_back(std::move(fp));
	while (recent_.size() > static_cast<size_t>(std::max(1, spec_.recentCapacity)))
		recent_.pop_front();
	return recent_.back().id;
}

void TemporalTracker::touch(uint64_t id, const Observation &obs)
{
	for (auto &fp : recent_)
		if (fp.id == id)
			fp.lastSeenMs = obs.timeMs;
}

bool TemporalTracker::debounced(int64_t now) const
{
	return haveEvent_ && now - lastEventMs_ < spec_.minEventIntervalMs;
}

TemporalDecision TemporalTracker::update(const Observation &obs)
{
	TemporalDecision d;
	const bool failing = !obs.passes && obs.confidence < threshold_ * spec_.releaseRatio;
	if (obs.passes) {
		++passStreak_;
		failStreak_ = 0;
	} else if (failing) {
		++failStreak_;
		passStreak_ = 0;
	} else {
		// Ambiguous frame (hysteresis band): neither confirms nor releases.
		passStreak_ = 0;
	}

	char buf[160];
	if (state_ == TrackState::Idle) {
		if (passStreak_ >= spec_.confirmFrames) {
			state_ = TrackState::Active;
			episodeMaxInstances_ = std::max(1, obs.instanceCount);
			higherFrames_ = lowerFrames_ = newTextFrames_ = 0;
			episodeText_ = obs.text;
			pendingText_.clear();
			int64_t age = 0;
			if (isRecentDuplicate(obs, &age)) {
				d.suppressed = true;
				std::snprintf(buf, sizeof(buf), "same message seen %lldms ago (cooldown %dms)",
					      static_cast<long long>(age), spec_.cooldownMs);
				d.suppressReason = buf;
				episodeFp_ = remember(obs);
			} else if (debounced(obs.timeMs)) {
				d.suppressed = true;
				d.suppressReason = "within minEventIntervalMs of previous event";
				episodeFp_ = remember(obs);
			} else {
				d.emitCount = useInstances_ ? std::clamp(obs.instanceCount, 1, spec_.maxBurst) : 1;
				std::snprintf(buf, sizeof(buf), "confirmed over %d frame(s)", passStreak_);
				d.reason = buf;
				episodeFp_ = remember(obs);
			}
		}
	} else {
		touch(episodeFp_, obs);
		if (failStreak_ >= spec_.releaseFrames) {
			state_ = TrackState::Idle;
			d.reason = "episode ended";
			d.state = state_;
			return d;
		}
		if (obs.passes && useInstances_) {
			if (obs.instanceCount > episodeMaxInstances_) {
				++higherFrames_;
				if (higherFrames_ >= spec_.confirmFrames) {
					const int delta = obs.instanceCount - episodeMaxInstances_;
					episodeMaxInstances_ = obs.instanceCount;
					higherFrames_ = 0;
					if (debounced(obs.timeMs)) {
						d.suppressed = true;
						d.suppressReason = "instance increase within minEventIntervalMs";
					} else {
						d.emitCount = std::min(delta, spec_.maxBurst);
						std::snprintf(buf, sizeof(buf), "instance count rose to %d",
							      obs.instanceCount);
						d.reason = buf;
						episodeFp_ = remember(obs);
					}
				}
			} else {
				higherFrames_ = 0;
			}
			if (obs.instanceCount < episodeMaxInstances_) {
				// Old entries expired; allow the next rise to count again.
				if (++lowerFrames_ >= spec_.releaseFrames) {
					episodeMaxInstances_ = std::max(1, obs.instanceCount);
					lowerFrames_ = 0;
				}
			} else {
				lowerFrames_ = 0;
			}
		} else if (obs.passes && !obs.text.empty() && !episodeText_.empty() &&
			   !sameMessage(obs.text, episodeText_, spec_.textSimilarity)) {
			if (!pendingText_.empty() && sameMessage(obs.text, pendingText_, spec_.textSimilarity))
				++newTextFrames_;
			else {
				pendingText_ = obs.text;
				newTextFrames_ = 1;
			}
			if (newTextFrames_ >= spec_.confirmFrames) {
				episodeText_ = obs.text;
				newTextFrames_ = 0;
				pendingText_.clear();
				int64_t age = 0;
				if (isRecentDuplicate(obs, &age)) {
					d.suppressed = true;
					d.suppressReason = "changed text matches a recent message";
				} else if (debounced(obs.timeMs)) {
					d.suppressed = true;
					d.suppressReason = "text change within minEventIntervalMs";
				} else {
					d.emitCount = 1;
					d.reason = "new message text";
					episodeFp_ = remember(obs);
				}
			}
		} else if (!obs.passes) {
			higherFrames_ = 0;
			newTextFrames_ = 0;
		} else {
			newTextFrames_ = 0;
			if (!obs.text.empty() && episodeText_.empty())
				episodeText_ = obs.text;
		}
	}
	if (d.emitCount > 0) {
		lastEventMs_ = obs.timeMs;
		haveEvent_ = true;
	}
	d.state = state_;
	return d;
}

} // namespace killsense
