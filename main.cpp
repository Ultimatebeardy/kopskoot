// killsense-replay - run a game profile against a folder of frames.
//
// Usage:
//   killsense-replay --profile data/profiles/sample-killfeed.json
//                    --frames tests/data/sequences/sample-killfeed
//                    [--fps 6] [--preset balanced] [--ocr sidecar|platform|none]
//                    [--threshold 0.8] [--expect N] [--verbose] [--json]
//
// Exit codes: 0 success (and --expect matched), 1 usage/IO error, 2 mismatch.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "killsense/fsutil.hpp"
#include "killsense/logging.hpp"
#include "killsense/replay.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace killsense;

static void usage()
{
	std::fprintf(
		stderr,
		"usage: killsense-replay --profile FILE --frames DIR [--fps N] [--preset low_impact|balanced|high_accuracy]\n"
		"                        [--ocr sidecar|platform|none] [--ocr-lang TAG] [--threshold X]\n"
		"                        [--gamertag NAME] [--user-templates DIR]\n"
		"                        [--cooldown MS] [--expect N] [--verbose] [--json] [--log-level LEVEL]\n");
}

int main(int argc, char **argv)
{
	ReplayOptions opt;
	int expect = -1;
	bool json = false;
	for (int i = 1; i < argc; ++i) {
		const std::string a = argv[i];
		auto next = [&](const char *name) -> const char * {
			if (i + 1 >= argc) {
				std::fprintf(stderr, "missing value for %s\n", name);
				std::exit(1);
			}
			return argv[++i];
		};
		if (a == "--profile")
			opt.profileFile = pathFromUtf8(next("--profile"));
		else if (a == "--frames")
			opt.framesDir = pathFromUtf8(next("--frames"));
		else if (a == "--fps")
			opt.fps = std::atof(next("--fps"));
		else if (a == "--preset") {
			if (!parsePreset(next("--preset"), opt.preset)) {
				usage();
				return 1;
			}
		} else if (a == "--ocr") {
			const std::string m = next("--ocr");
			if (m == "none")
				opt.ocr = ReplayOcrMode::None;
			else if (m == "sidecar")
				opt.ocr = ReplayOcrMode::Sidecar;
			else if (m == "platform")
				opt.ocr = ReplayOcrMode::Platform;
			else {
				usage();
				return 1;
			}
		} else if (a == "--ocr-lang")
			opt.ocrLanguage = next("--ocr-lang");
		else if (a == "--gamertag")
			opt.personalization.gamertag = next("--gamertag");
		else if (a == "--user-templates")
			opt.personalization.userTemplateDir = killsense::pathFromUtf8(next("--user-templates"));
		else if (a == "--threshold")
			opt.overrides.threshold = std::atof(next("--threshold"));
		else if (a == "--cooldown")
			opt.overrides.cooldownMs = std::atoi(next("--cooldown"));
		else if (a == "--expect")
			expect = std::atoi(next("--expect"));
		else if (a == "--verbose")
			opt.collectFrameLog = true;
		else if (a == "--json")
			json = true;
		else if (a == "--log-level") {
			LogLevel lvl;
			if (parseLogLevel(next("--log-level"), lvl))
				setLogLevel(lvl);
		} else if (a == "--help" || a == "-h") {
			usage();
			return 0;
		} else {
			std::fprintf(stderr, "unknown argument: %s\n", a.c_str());
			usage();
			return 1;
		}
	}
	if (opt.profileFile.empty() || opt.framesDir.empty()) {
		usage();
		return 1;
	}

	const ReplayResult r = runReplay(opt);
	if (!r.ok) {
		std::fprintf(stderr, "replay failed: %s\n", r.error.c_str());
		return 1;
	}
	for (const auto &line : r.frameLog)
		std::printf("%s\n", line.c_str());
	for (const auto &ev : r.events) {
		if (json)
			std::printf("%s\n", toJson(ev, true).c_str());
		else
			std::printf("EVENT %-9s t=%6lldms conf=%.3f kills=%lld streak=%lld target=%s (%s)\n",
				    eventTypeId(ev.type), static_cast<long long>(ev.timestampMs), ev.confidence,
				    static_cast<long long>(ev.sessionKills), static_cast<long long>(ev.killStreak),
				    ev.target.c_str(), ev.reason.c_str());
	}
	std::printf("frames=%d events=%zu kills=%lld deaths=%lld bestStreak=%lld\n", r.frames, r.events.size(),
		    static_cast<long long>(r.session.kills), static_cast<long long>(r.session.deaths),
		    static_cast<long long>(r.session.bestStreak));
	if (expect >= 0 && r.session.kills != expect) {
		std::fprintf(stderr, "expected %d kill(s), got %lld\n", expect,
			     static_cast<long long>(r.session.kills));
		return 2;
	}
	return 0;
}
