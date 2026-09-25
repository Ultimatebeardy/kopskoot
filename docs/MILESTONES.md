# Milestone Plan

## Milestone 1 — Reliable architecture (this delivery)
| Requirement | Status |
|---|---|
| Loads inside OBS, "REALM KillSense" dock | ✅ (verified in OBS 30.2/Linux) |
| Select an OBS video/game source (inputs and scenes) | ✅ |
| Safe frame sampling (tick callback, GPU ROI crop, async staging, bounded queue, drop-oldest) | ✅ |
| Normalized ROIs (profiles + custom ROI override in Settings) | ✅ |
| Worker-thread processing | ✅ |
| Template detection + OCR (Windows.Media.Ocr) + colour + HUD change | ✅ (OCR backend Windows-only, not yet compiled on Windows) |
| Evidence fusion + temporal filtering + duplicate suppression + rapid kills | ✅ |
| Simulated kill indicator recognised, counter increments exactly once | ✅ |
| Event shown in dock, KillSense event emitted (event bus, obs-websocket vendor, JSONL) | ✅ |
| Test Kill Event, Reset Session, hotkeys | ✅ |
| Save/load settings (versioned config.json) | ✅ |
| Debug View, Profile Manager (list/reload/open folders/errors), Settings, Event Outputs | ✅ |
| Auto Detect (reads OBS capture settings only) | ✅ |
| Unit tests, replay tool, headless libobs harness, websocket E2E script | ✅ |
| Clean unload | ✅ |

### Milestone 1.1 — Player-specific counting (done)
- Gamertag setting + `{gamertag}` profile placeholder; per-user templates captured from the dock ("Capture my name plate").
- Kill-feed role matching (your name as killer vs victim), `requireAll`, `vetoWhenActive` (dead/spectating screens block kills).
- Deaths and K/D in the dock; free text-source counter; THE FINALS profile v6.

## Milestone 2 — Learn This Game
- Low-res full-frame sampler slot (e.g. 320×180 gray at 6 fps) feeding `CalibrationRecorder` (**core done + tested**).
- "Mark kill" hotkey; progress UI; per-sample accept/reject.
- `analyzeCandidates` (**core done + tested**) → candidate ROIs drawn over a preview; drag/resize editor.
- Collect text (OCR) and icon crops inside accepted ROIs; auto-cut template(s); propose keywords.
- Generate and save a user profile; run a replay of the collected samples to validate it.
- All local; no uploads.

## Milestone 3 — Outputs & automation
- Optional localhost-only (127.0.0.1) HTTP/SSE or WebSocket event output with a random token; off by default.
- Streamer.bot integration pack (import string / sample actions: play random video on kill, streak alerts).
- Derived announcements (double/triple/multi kill *windows*) as an explicit opt-in, clearly labelled as KillSense-derived.
- OBS actions output (e.g. restart a media source on kill) for users without Streamer.bot.

## Milestone 4 — Generic visual classification
- ONNX Runtime (MIT) behind an `IInferenceBackend`: CPU EP always, DirectML EP (any DX12 GPU) and CUDA/TensorRT EP optional; selected at runtime, falls back to CPU.
- Small classifier over ROI crops (kill-feed entry / medal / banner vs. background) trained from Learn This Game samples + public, licence-clean data.
- `onnx` detector kind is already in the profile schema and reports *unavailable* until then.

## Milestone 5 — Distribution
- Windows installer (Inno Setup / WiX) and signed DLL; GitHub Actions release via the template workflows.
- More built-in profiles (community-contributed, validated with replay sequences).
- macOS (Apple Vision OCR) and Linux OCR backend (Tesseract, Apache-2.0) as optional components.
- Opt-in, anonymous, aggregate-only telemetry *if* ever needed (currently none exists).
