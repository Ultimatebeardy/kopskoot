# REALM KillSense

A native OBS Studio plugin that detects kills in your gameplay from the video OBS already captures, then turns them into stream events: obs-websocket vendor events (Streamer.bot and similar), a JSON Lines file, and an internal event bus. It runs locally and never touches the game: no memory reading, no injection, no network interception, no uploads.

![Dock during a live test](docs/images/dock-live.png)

## Status: Milestone 1

- Dock: source and profile selection, Auto Detect, Start/Stop, Test Kill Event, Reset Session, live stats (FPS, latency, dropped frames, CPU, OCR/GPU state), Debug View, Settings/Event Outputs, Profile Manager.
- Capture: an OBS tick callback crops only the profile's regions on the GPU, reads them back asynchronously and passes them to a worker thread through a bounded queue. When the worker is busy, frames are dropped rather than slowing OBS down.
- Detection:
  - Detectors: template/icon, OCR (Windows.Media.Ocr), colour and HUD change.
  - Evidence fusion requires at least `minSignals` signals, so OCR alone never proves a kill.
  - A temporal tracker counts a message that stays on screen once, still counts rapid consecutive kills, and ignores flicker.
- Profiles: JSON files for new games with no recompiling (`docs/PROFILE_SCHEMA.md`). Ships with `generic.json`, `sample-killfeed.json` and `the-finals.json`.
- Per-player setup: enter **your gamertag** in Settings and press **Capture my name plate** once in a match. Profiles use `{gamertag}` and per-user images so only *your* kills count, not kills you watch while spectating. Nothing is uploaded; the captured image stays in the OBS config folder.
- Kills, deaths and K/D in the dock, in every event and in `GetStatus`.
- Free on-screen counter: pick any OBS Text source in Settings → Event Outputs and KillSense keeps it updated (`KILLS {kills} | DEATHS {deaths} | K/D {kd}`). Styled/animated overlays are separate Browser Sources that listen to the obs-websocket events (see `docs/EVENT_SCHEMA.md`).
- Tests: 70 unit tests, a replay tool for recorded frames, a headless libobs harness and an end-to-end obs-websocket script.
- Planned: Learn This Game UI (M2; the core logic is already built and tested), localhost output and a Streamer.bot pack (M3), ONNX visual classification (M4). See `docs/MILESTONES.md`.

> **Verification status.** The core, the tests and the plugin were compiled and run on Linux against real OBS 30.2 (details in `docs/TESTING.md`). The Windows/MSVC build and the Windows OCR backend have **not yet been compiled** in this environment. Build on Windows first and report any compiler errors.

## Quick start (Windows)
```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
cmake --install build_x64 --config RelWithDebInfo     # elevated prompt
```
Restart OBS, open **Docks → REALM KillSense**, then follow the acceptance test in `docs/TESTING.md` §4.

## Documentation
| Doc | Contents |
|---|---|
| [ARCHITECTURE](docs/ARCHITECTURE.md) | Components, pipeline, threading/frame model, fusion and temporal logic, verified OBS APIs |
| [BUILDING](docs/BUILDING.md) | Exact Windows build/install commands, Debug vs Release, install paths |
| [TESTING](docs/TESTING.md) | Unit tests, replay mode, libobs harness, in-OBS acceptance procedure |
| [PROFILE_SCHEMA](docs/PROFILE_SCHEMA.md) | Every profile field and how to create a profile for a new game |
| [EVENT_SCHEMA](docs/EVENT_SCHEMA.md) | Event JSON, event types, obs-websocket vendor events/requests |
| [DEPENDENCIES](docs/DEPENDENCIES.md) | Libraries, licences, how CMake obtains them |
| [MILESTONES](docs/MILESTONES.md) | Roadmap |
| [FILE_MANIFEST](docs/FILE_MANIFEST.md) | Every file in the repository and its purpose |

## Privacy
Detection runs on your PC. KillSense reads only the pixels of the OBS source you select, and only inside the profile's regions of interest. It has no telemetry. Auto Detect reads only the window name that your own Game Capture source is set to.

## License
GPL-2.0-or-later (see `LICENSE`), like OBS Studio. Third-party notices: `THIRD_PARTY_NOTICES.md`.
