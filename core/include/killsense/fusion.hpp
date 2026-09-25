// REALM KillSense - evidence fusion.
// Combines detector signals into one confidence per target:
//   confidence = sum(weight_i * score_i) / sum(weight_i)   over AVAILABLE signals
// A target "passes" only if confidence >= threshold AND at least minSignals
// signals are active AND (if requireAny is set) one of those detectors is
// active AND no veto was raised. OCR alone therefore never proves a kill
// unless a profile explicitly allows it (minSignals = 1).
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "killsense/detector.hpp"
#include "killsense/event.hpp"
#include "killsense/profile.hpp"

#include <string>
#include <vector>

namespace killsense {

struct Contribution {
	std::string detector;
	DetectorKind kind = DetectorKind::Template;
	double weight = 0.0;
	double score = 0.0;
	double contribution = 0.0; // weight * score / sum(weights)
	bool active = false;
	bool available = true;
	bool veto = false;
	std::string detail;
};

struct FusionResult {
	double confidence = 0.0;
	int activeSignals = 0;
	int availableSignals = 0;
	double availableWeight = 0.0;
	bool vetoed = false;
	std::string vetoReason;
	bool requireAnySatisfied = true;
	bool passes = false;
	int instanceCount = 0;
	std::vector<Contribution> contributions;
	std::string reason; // why it passed / failed
};

// `outputs` must contain one entry per profile detector (by id).
FusionResult fuseTarget(const TargetSpec &target, const std::vector<DetectorOutput> &outputs);

const DetectorOutput *findOutput(const std::vector<DetectorOutput> &outputs, const std::string &detectorId);

} // namespace killsense
