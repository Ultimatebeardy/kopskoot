# File Manifest

Every file in the repository and its purpose. Keep this updated when files are added so later changes extend the existing project instead of replacing it.

| Path | Purpose |
|---|---|
| `.clang-format` | Template C/C++ style |
| `.gersemirc` | Template CMake style |
| `.gitignore` | Template ignore rules + KillSense folders |
| `CMakeLists.txt` | Root build (obs-plugintemplate based): plugin MODULE target, options, links core/libobs/frontend/Qt, optional libobs harness |
| `CMakePresets.json` | Template presets (windows-x64, ubuntu, macos) with frontend API + Qt enabled |
| `LICENSE` | GPL-2.0 (from template) |
| `README.md` | Overview, status, quick start |
| `THIRD_PARTY_NOTICES.md` | Third-party licences |
| `buildspec.json` | Plugin name/version and pinned OBS 31.1.1 SDK / obs-deps / Qt6 (SHA-256) |
| `.github/actions/build-plugin/action.yaml` | From obs-plugintemplate (unchanged) |
| `.github/actions/check-changes/action.yaml` | From obs-plugintemplate (unchanged) |
| `.github/actions/package-plugin/action.yaml` | From obs-plugintemplate (unchanged) |
| `.github/actions/run-clang-format/action.yaml` | From obs-plugintemplate (unchanged) |
| `.github/actions/run-gersemi/action.yaml` | From obs-plugintemplate (unchanged) |
| `.github/actions/setup-macos-codesigning/action.yaml` | From obs-plugintemplate (unchanged) |
| `.github/scripts/.Aptfile` | From obs-plugintemplate (unchanged) |
| `.github/scripts/.Brewfile` | From obs-plugintemplate (unchanged) |
| `.github/scripts/Build-Windows.ps1` | From obs-plugintemplate (unchanged) |
| `.github/scripts/Package-Windows.ps1` | From obs-plugintemplate (unchanged) |
| `.github/scripts/build-macos` | From obs-plugintemplate (unchanged) |
| `.github/scripts/build-ubuntu` | From obs-plugintemplate (unchanged) |
| `.github/scripts/package-macos` | From obs-plugintemplate (unchanged) |
| `.github/scripts/package-ubuntu` | From obs-plugintemplate (unchanged) |
| `.github/scripts/utils.pwsh/Ensure-Location.ps1` | From obs-plugintemplate (unchanged) |
| `.github/scripts/utils.pwsh/Invoke-External.ps1` | From obs-plugintemplate (unchanged) |
| `.github/scripts/utils.pwsh/Logger.ps1` | From obs-plugintemplate (unchanged) |
| `.github/scripts/utils.zsh/check_macos` | From obs-plugintemplate (unchanged) |
| `.github/scripts/utils.zsh/check_ubuntu` | From obs-plugintemplate (unchanged) |
| `.github/scripts/utils.zsh/log_debug` | From obs-plugintemplate (unchanged) |
| `.github/scripts/utils.zsh/log_error` | From obs-plugintemplate (unchanged) |
| `.github/scripts/utils.zsh/log_group` | From obs-plugintemplate (unchanged) |
| `.github/scripts/utils.zsh/log_info` | From obs-plugintemplate (unchanged) |
| `.github/scripts/utils.zsh/log_output` | From obs-plugintemplate (unchanged) |
| `.github/scripts/utils.zsh/log_status` | From obs-plugintemplate (unchanged) |
| `.github/scripts/utils.zsh/log_warning` | From obs-plugintemplate (unchanged) |
| `.github/scripts/utils.zsh/mkcd` | From obs-plugintemplate (unchanged) |
| `.github/scripts/utils.zsh/set_loglevel` | From obs-plugintemplate (unchanged) |
| `.github/scripts/utils.zsh/setup_ubuntu` | From obs-plugintemplate (unchanged) |
| `.github/workflows/build-project.yaml` | From obs-plugintemplate (unchanged) |
| `.github/workflows/check-format.yaml` | From obs-plugintemplate (unchanged) |
| `.github/workflows/dispatch.yaml` | From obs-plugintemplate (unchanged) |
| `.github/workflows/pr-pull.yaml` | From obs-plugintemplate (unchanged) |
| `.github/workflows/push.yaml` | From obs-plugintemplate (unchanged) |
| `build-aux/.run-format.zsh` | From obs-plugintemplate (unchanged) |
| `build-aux/run-clang-format` | From obs-plugintemplate (unchanged) |
| `build-aux/run-gersemi` | From obs-plugintemplate (unchanged) |
| `build-aux/run-swift-format` | From obs-plugintemplate (unchanged) |
| `build-aux/.functions/log_debug` | From obs-plugintemplate (unchanged) |
| `build-aux/.functions/log_error` | From obs-plugintemplate (unchanged) |
| `build-aux/.functions/log_group` | From obs-plugintemplate (unchanged) |
| `build-aux/.functions/log_info` | From obs-plugintemplate (unchanged) |
| `build-aux/.functions/log_output` | From obs-plugintemplate (unchanged) |
| `build-aux/.functions/log_status` | From obs-plugintemplate (unchanged) |
| `build-aux/.functions/log_warning` | From obs-plugintemplate (unchanged) |
| `build-aux/.functions/set_loglevel` | From obs-plugintemplate (unchanged) |
| `cmake/.CMakeBuildNumber` | From obs-plugintemplate (unchanged) |
| `cmake/common/bootstrap.cmake` | From obs-plugintemplate (unchanged) |
| `cmake/common/buildnumber.cmake` | From obs-plugintemplate (unchanged) |
| `cmake/common/buildspec_common.cmake` | From obs-plugintemplate (unchanged) |
| `cmake/common/ccache.cmake` | From obs-plugintemplate (unchanged) |
| `cmake/common/compiler_common.cmake` | From obs-plugintemplate (unchanged) |
| `cmake/common/helpers_common.cmake` | From obs-plugintemplate (unchanged) |
| `cmake/common/osconfig.cmake` | From obs-plugintemplate (unchanged) |
| `cmake/linux/compilerconfig.cmake` | From obs-plugintemplate (unchanged) |
| `cmake/linux/defaults.cmake` | From obs-plugintemplate (unchanged) |
| `cmake/linux/helpers.cmake` | From obs-plugintemplate (unchanged) |
| `cmake/macos/buildspec.cmake` | From obs-plugintemplate (unchanged) |
| `cmake/macos/compilerconfig.cmake` | From obs-plugintemplate (unchanged) |
| `cmake/macos/defaults.cmake` | From obs-plugintemplate (unchanged) |
| `cmake/macos/helpers.cmake` | From obs-plugintemplate (unchanged) |
| `cmake/macos/xcode.cmake` | From obs-plugintemplate (unchanged) |
| `cmake/macos/resources/ccache-launcher-c.in` | From obs-plugintemplate (unchanged) |
| `cmake/macos/resources/ccache-launcher-cxx.in` | From obs-plugintemplate (unchanged) |
| `cmake/macos/resources/create-package.cmake.in` | From obs-plugintemplate (unchanged) |
| `cmake/macos/resources/distribution.in` | From obs-plugintemplate (unchanged) |
| `cmake/macos/resources/installer-macos.pkgproj.in` | From obs-plugintemplate (unchanged) |
| `cmake/windows/buildspec.cmake` | From obs-plugintemplate (unchanged) |
| `cmake/windows/compilerconfig.cmake` | From obs-plugintemplate (unchanged) |
| `cmake/windows/defaults.cmake` | From obs-plugintemplate (unchanged) |
| `cmake/windows/helpers.cmake` | From obs-plugintemplate (unchanged) |
| `cmake/windows/resources/resource.rc.in` | From obs-plugintemplate (unchanged) |
| `core/CMakeLists.txt` | killsense-core static library; standalone-buildable; nlohmann/json FetchContent; Windows OCR detection |
| `core/include/killsense/calibration.hpp` | Learn This Game core API |
| `core/include/killsense/capture.hpp` | Presets, capture planning, FramePacket, FramePool |
| `core/include/killsense/config.hpp` | Versioned plugin config |
| `core/include/killsense/detector.hpp` | IDetector interface, DetectorOutput, TemplateMatcher |
| `core/include/killsense/engine.hpp` | DetectionEngine pipeline API |
| `core/include/killsense/event-bus.hpp` | Event bus + IEventOutput + JSONL output |
| `core/include/killsense/event.hpp` | Event types and KillEvent schema |
| `core/include/killsense/fsutil.hpp` | UTF-8 path helpers, atomic file writes |
| `core/include/killsense/fusion.hpp` | Evidence fusion API |
| `core/include/killsense/geometry.hpp` | NormRect/PixelRect, normalized<->pixel conversion, IoU, aspect parsing |
| `core/include/killsense/image.hpp` | Image type, gray/crop/resize, dHash, file IO |
| `core/include/killsense/logging.hpp` | Logging facade (levels, pluggable sink) |
| `core/include/killsense/ocr.hpp` | IOcrEngine interface, Null/Scripted engines, platform factory |
| `core/include/killsense/profile.hpp` | Game profile model, parser, overrides, registry |
| `core/include/killsense/replay.hpp` | Replay API |
| `core/include/killsense/session.hpp` | Session counters API |
| `core/include/killsense/temporal.hpp` | Temporal tracker API |
| `core/include/killsense/text-match.hpp` | OCR-tolerant text matching |
| `core/include/killsense/killfeed.hpp` | Kill-feed name/role (killer vs victim) matching |
| `core/include/killsense/user-template.hpp` | Name-plate auto-crop for per-user templates |
| `core/include/killsense/version.hpp` | Schema versions, vendor name |
| `core/include/killsense/worker.hpp` | Bounded queue + analysis worker thread |
| `core/src/calibration.cpp` | Rolling buffer, marked samples, candidate ROI analysis |
| `core/src/capture.cpp` | Capture planning implementation |
| `core/src/config.cpp` | Config JSON load/save with fallback |
| `core/src/engine.cpp` | Per-frame pipeline, rule classification, makeKillEvent |
| `core/src/event-bus.cpp` | Dispatch thread, outputs |
| `core/src/event.cpp` | Event JSON (de)serialization, timestamps |
| `core/src/fsutil.cpp` | fsutil implementation |
| `core/src/fusion.cpp` | Weighted confidence, minSignals, requireAny, veto |
| `core/src/image.cpp` | Image ops + stb integration |
| `core/src/logging.cpp` | Logging implementation |
| `core/src/profile.cpp` | Validating profile parser and registry |
| `core/src/replay.cpp` | Offline replay of frame sequences |
| `core/src/session.cpp` | Kills/streak/deaths counting |
| `core/src/temporal.cpp` | Confirmation, release, duplicate suppression, rapid kills |
| `core/src/text-match.cpp` | Normalization, edit distance, sameMessage, regex |
| `core/src/killfeed.cpp` | Kill-feed role matching |
| `core/src/user-template.cpp` | Name-plate auto-crop |
| `core/src/worker.cpp` | Worker thread, stats, debug snapshot |
| `core/src/detectors/basic-detectors.cpp` | Colour + HUD-change detectors, detector factory |
| `core/src/detectors/detectors.hpp` | Built-in detector classes (internal) |
| `core/src/detectors/ocr-detector.cpp` | OCR detector (keywords, patterns, negative words, caching) |
| `core/src/detectors/template-detector.cpp` | Template/icon detector |
| `core/src/detectors/template-matcher.cpp` | Coarse-to-fine NCC template matching, HSV |
| `core/src/ocr/ocr-common.cpp` | Scripted OCR + non-Windows factory |
| `core/src/ocr/windows-ocr-engine.cpp` | Windows.Media.Ocr backend (C++/WinRT) |
| `data/locale/en-US.ini` | English UI strings |
| `data/profiles/generic.json` | Generic fallback profile |
| `data/profiles/sample-killfeed.json` | Reference/test profile |
| `data/profiles/wows-legends.json` | World of Warships: Legends profile (DESTROYED ribbon) |
| `data/profiles/the-finals.json` | THE FINALS profile (gamertag + name plate + kill feed) |
| `data/templates/sample-kill-icon.png` | Template for the sample profile |
| `data/templates/the-finals-eliminated.png` | THE FINALS "ELIMINATED" banner word |
| `data/test-assets/killsense-test-background.png` | Overlay/background images for the in-OBS test scene |
| `data/test-assets/killsense-test-death.png` | Overlay/background images for the in-OBS test scene |
| `data/test-assets/killsense-test-kill-row1.png` | Overlay/background images for the in-OBS test scene |
| `data/test-assets/killsense-test-kill-row2.png` | Overlay/background images for the in-OBS test scene |
| `docs/ARCHITECTURE.md` | Documentation |
| `docs/BUILDING.md` | Documentation |
| `docs/DEPENDENCIES.md` | Documentation |
| `docs/EVENT_SCHEMA.md` | Documentation |
| `docs/MILESTONES.md` | Documentation |
| `docs/PROFILE_SCHEMA.md` | Documentation |
| `docs/TESTING.md` | Documentation |
| `docs/images/debug-view.png` | Screenshots from the verified OBS run |
| `docs/images/dock-live.png` | Screenshots from the verified OBS run |
| `src/controller.cpp` | Owns all components; lifecycle, hotkeys, config, events |
| `src/controller.hpp` | Controller API and thread ownership |
| `src/plugin-main.cpp` | OBS module entry: load/post_load/unload, dock registration, frontend events, log bridge |
| `src/plugin-support.c.in` | Template logging helper (configured) |
| `src/plugin-support.h` | Template logging helper declaration |
| `src/obs/frame-sampler.cpp` | Tick callback, texrender, staging, readback |
| `src/obs/frame-sampler.hpp` | GPU ROI sampler API |
| `src/obs/obs-raii.hpp` | RAII wrappers for OBS references |
| `src/obs/obs-sources.cpp` | Source enumeration, capture-target parsing, profile matching |
| `src/obs/obs-sources.hpp` | Source listing + Auto Detect API |
| `src/outputs/websocket-vendor-output.cpp` | Vendor registration, events, requests |
| `src/outputs/websocket-vendor-output.hpp` | obs-websocket vendor output API |
| `src/ui/debug-view.cpp` | ROI overlay rendering and analysis text |
| `src/ui/debug-view.hpp` | Debug View dialog |
| `src/ui/killsense-dock.cpp` | Dock UI and live status |
| `src/ui/killsense-dock.hpp` | Dock widget |
| `src/ui/profile-manager-dialog.cpp` | Profile list, issues, reload, open folders |
| `src/ui/profile-manager-dialog.hpp` | Profile Manager dialog |
| `src/ui/settings-dialog.cpp` | Detection/Outputs/Advanced settings |
| `src/ui/settings-dialog.hpp` | Settings dialog |
| `src/ui/capture-dialog.cpp` | "Capture my name plate" dialog |
| `src/ui/capture-dialog.hpp` | Capture dialog |
| `src/ui/ui-common.hpp` | Qt helpers (localization, string conversion) |
| `tests/CMakeLists.txt` | GoogleTest setup, test registration, CLI replay test |
| `tests/test-calibration.cpp` | Unit tests (see docs/TESTING.md) |
| `tests/test-config.cpp` | Unit tests (see docs/TESTING.md) |
| `tests/test-event-session.cpp` | Unit tests (see docs/TESTING.md) |
| `tests/test-fusion.cpp` | Unit tests (see docs/TESTING.md) |
| `tests/test-geometry-capture.cpp` | Unit tests (see docs/TESTING.md) |
| `tests/test-killfeed-personal.cpp` | Unit tests: kill feed roles, gamertag personalisation, requireAll, name-plate crop |
| `tests/test-helpers.hpp` | Test utilities |
| `tests/test-profile.cpp` | Unit tests (see docs/TESTING.md) |
| `tests/test-replay.cpp` | Unit tests (see docs/TESTING.md) |
| `tests/test-template-match.cpp` | Unit tests (see docs/TESTING.md) |
| `tests/test-temporal.cpp` | Unit tests (see docs/TESTING.md) |
| `tests/test-text-match.cpp` | Unit tests (see docs/TESTING.md) |
| `tests/test-worker.cpp` | Unit tests (see docs/TESTING.md) |
| `tests/data/entries/death-raider.png` | Kill-feed entry strips for template tests |
| `tests/data/entries/kill-bandit.png` | Kill-feed entry strips for template tests |
| `tests/data/entries/kill-raider.png` | Kill-feed entry strips for template tests |
| `tests/data/entries/kill-scout.png` | Kill-feed entry strips for template tests |
| `tests/data/entries/kill-warden.png` | Kill-feed entry strips for template tests |
| `third_party/obs-websocket-api/obs-websocket-api.h` | Vendored obs-websocket API header |
| `third_party/stb/stb_image.h` | Vendored image loader |
| `third_party/stb/stb_image_write.h` | Vendored PNG writer |
| `tools/assets/generate_test_assets.py` | Generates templates, OBS test overlays and replay sequences |
| `tools/libobs-harness/main.cpp` | Headless libobs integration harness |
| `tools/replay/CMakeLists.txt` | Replay tool target |
| `tools/replay/main.cpp` | killsense-replay CLI |
| `tools/websocket/killsense_ws_test.py` | obs-websocket end-to-end test / client example |
| `tests/data/sequences/sample-killfeed/` | 232 files: 116 recorded frames (PNG) + OCR sidecars (.ocr.txt) for replay tests |
