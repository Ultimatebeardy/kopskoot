// SPDX-License-Identifier: GPL-2.0-or-later
#include "killsense/fusion.hpp"

#include <algorithm>
#include <cstdio>

namespace killsense {

const DetectorOutput *findOutput(const std::vector<DetectorOutput> &outputs, const std::string &detectorId)
{
	for (const auto &o : outputs)
		if (o.detectorId == detectorId)
			return &o;
	return nullptr;
}

FusionResult fuseTarget(const TargetSpec &target, const std::vector<DetectorOutput> &outputs)
{
	FusionResult r;
	double weighted = 0.0;
	for (const auto &sig : target.signalSpecs) {
		Contribution c;
		c.detector = sig.detector;
		c.weight = sig.weight;
		const DetectorOutput *o = findOutput(outputs, sig.detector);
		if (!o || !o->available) {
			c.available = false;
			c.detail = o ? o->detail : "detector missing";
			r.contributions.push_back(std::move(c));
			continue;
		}
		c.kind = o->kind;
		c.score = std::clamp(o->score, 0.0, 1.0);
		c.active = o->active;
		c.detail = o->detail;
		r.availableWeight += sig.weight;
		++r.availableSignals;
		weighted += sig.weight * c.score;
		if (c.active && sig.weight > 0.0)
			++r.activeSignals;
		if (sig.veto && o->veto) {
			c.veto = true;
			if (!r.vetoed) {
				r.vetoed = true;
				r.vetoReason = sig.detector + ": " + o->vetoReason;
			}
		}
		if (sig.countInstances)
			r.instanceCount = std::max(r.instanceCount, o->instances);
		r.contributions.push_back(std::move(c));
	}
	if (r.availableWeight > 0.0) {
		r.confidence = weighted / r.availableWeight;
		for (auto &c : r.contributions)
			if (c.available)
				c.contribution = c.weight * c.score / r.availableWeight;
	}
	if (!target.requireAny.empty()) {
		r.requireAnySatisfied = false;
		for (const auto &id : target.requireAny) {
			const DetectorOutput *o = findOutput(outputs, id);
			if (o && o->available && o->active) {
				r.requireAnySatisfied = true;
				break;
			}
		}
	}
	if (r.vetoed)
		r.confidence = 0.0;

	char buf[160];
	if (r.availableSignals == 0) {
		r.reason = "no available signals";
	} else if (r.vetoed) {
		r.reason = "vetoed by " + r.vetoReason;
	} else if (r.confidence < target.threshold) {
		std::snprintf(buf, sizeof(buf), "confidence %.2f < threshold %.2f", r.confidence, target.threshold);
		r.reason = buf;
	} else if (r.activeSignals < target.minSignals) {
		std::snprintf(buf, sizeof(buf), "%d active signal(s) < minSignals %d", r.activeSignals,
			      target.minSignals);
		r.reason = buf;
	} else if (!r.requireAnySatisfied) {
		r.reason = "none of requireAny detectors active";
	} else {
		r.passes = true;
		std::snprintf(buf, sizeof(buf), "confidence %.2f with %d active signal(s)", r.confidence,
			      r.activeSignals);
		r.reason = buf;
	}
	if (r.instanceCount == 0 && r.passes)
		r.instanceCount = 1;
	return r;
}

} // namespace killsense
