# KopSkoot — Technical Architecture

KopSkoot is a native OBS Studio plugin (C++17, libobs, Qt 6) that detects kills from the video frames OBS already has. It never touches the game: no memory reading, no injection, no network interception, no game-file changes. Everything runs locally.

## 1. Component overview

```
┌──────────────────────────── OBS Studio process ─────────────────────────────┐
│                                                                              │
│  OBS graphics thread                      KillSense analysis thread          │
│  ┌─────────────────────┐   FramePacket   ┌──────────────────────────────┐   │
│  │ FrameSampler (tick) │ ──────────────► │ AnalysisWorker               │   │
│  │  • ROI texrender    │ bounded queue   │  DetectionEngine             │   │
│  │  • stage / map      │  (cap 2, drop   │   ROI preprocessing          │   │
│  └─────────────────────┘   oldest)       │   Detectors (template, OCR,  │   │
│            ▲  FramePool (recycled        │     colour, HUD change, …)   │   │
│            │  buffers)                   │   Evidence fusion            │   │
│            │                             │   Temporal tracker           │   │
│  UI thread (Qt)                          └──────────────┬───────────────┘   │
│  ┌─────────────────────┐                                │ DetectedEvent     │
│  │ KopSkoot dock│◄── snapshot() ── Controller ◄──┘                   │
│  │ Debug View/Settings │                  │  SessionTracker (kills/streak)  │
│  └─────────────────────┘                  ▼                                  │
│                                   EventBus dispatch thread                   │
│                                   ├─ obs-websocket vendor "RealmKillSense"   │
│                                   ├─ JSONL file (optional)                   │
│                                   └─ internal subscribers                    │
└──────────────────────────────────────────────────────────────────────────────┘
```

Two layers:

| Layer | Location | Depends on OBS? | Purpose |
|---|---|---|---|
| **killsense-core** | `core/` | **No** | Images, ROI maths, profiles, detectors, OCR interface, fusion, temporal logic, session, events, event bus, config, calibration core, replay. Unit-tested and usable from the replay tool. |
| **Plugin** | `src/` | Yes | libobs frame sampling, source enumeration, obs-websocket vendor, controller, Qt dock/dialogs, hotkeys, frontend events. |

## 2. Detection pipeline

```
OBS source ─► Frame Sampler ─► Profile Resolver ─► ROI Extractor ─► Preprocessing
          ─► Detection Modules ─► Evidence Fusion ─► Temporal Confirmation
          ─► Duplicate Suppression ─► Kill Event ─► Session counters ─► Outputs
```

| Stage | Implementation |
|---|---|
| Frame sampler | `src/obs/frame-sampler.*` — tick callback, samples at the preset FPS (default 6), only when the worker can accept work. |
| Profile resolver | `Controller::rebuildActiveProfile` + `obs-sources.cpp::autoDetectProfile` (Auto Detect reads only OBS's own Game/Window Capture settings). |
| ROI extractor | `core/capture.cpp::planCaptures` → the **GPU** crops each region with an orthographic projection over the ROI and downsamples to reference scale. Only ROI pixels are ever copied to the CPU. |
| Preprocessing | `DetectionEngine::process` — gray conversion and 64-bit dHash per region, once per frame. |
| Detectors | `core/src/detectors/*` behind `IDetector` (`core/include/killsense/detector.hpp`). |
| Evidence fusion | `core/src/fusion.cpp` — weighted confidence, `minSignals`, `requireAny`, vetoes. |
| Temporal confirmation + duplicate suppression | `core/src/temporal.cpp` — `TemporalTracker` state machine. |
| Event classification | `DetectionEngine::classify` — profile `rules` (e.g. OCR "headshot" ⇒ Headshot). No guessing: default is the target's event type. |
| Session | `core/src/session.cpp` — kills, streak, best streak, deaths, sequence. |
| Outputs | `core/src/event-bus.cpp` + `src/outputs/websocket-vendor-output.cpp`. |

### Detector modules (Milestone 1)

| Kind | Signal | Notes |
|---|---|---|
| `template` | Normalized cross-correlation of an icon template. Coarse-to-fine search with integral images. Reports **instances** (distinct matches) for rapid-kill counting. | Templates are resolution-independent: scaled by `pxPerReference`. |
| `ocr` | Keyword (fuzzy, OCR-error tolerant), regex patterns, **negative words ⇒ veto**. | Backend: Windows.Media.Ocr (built into Windows 10/11). OCR re-runs only when the region's dHash changes (or `refreshMs` elapses) and never faster than the preset allows. |
| `color` | Fraction of ROI pixels inside an HSV range. | Cheap layout/colour cue. |
| `hud_change` | Mean absolute difference vs. previous frame; evidence decays over `holdMs`. | "Something appeared in the HUD just now." |
| `onnx` | **Planned (M4).** Parsed and reported *unavailable*; never produces fake evidence. | Execution-provider abstraction planned (CPU / DirectML / CUDA). |

### Evidence fusion

```
confidence = Σ(weight_i × score_i) / Σ(weight_i)      over AVAILABLE signals only
passes     = confidence ≥ threshold
           ∧ activeSignals ≥ minSignals
           ∧ (requireAny empty ∨ one requireAny detector active)
           ∧ no veto
```
OCR alone never proves a kill unless a profile explicitly sets `minSignals: 1`. Unavailable detectors (e.g. OCR on a PC without an OCR language pack) are excluded from the denominator and shown in the dock/Debug View.

### Temporal tracker (one per target)

```
Idle ──(passes for confirmFrames)──► Active  ⇒ emit (unless duplicate / debounced)
Active ──(fails for releaseFrames)──► Idle
```
- *Fails* = not passing **and** confidence < `threshold × releaseRatio` (hysteresis band ignores flicker).
- While Active, **new** kills are still detected:
  - instance count rises (e.g. a second kill-feed icon) and stays risen for `confirmFrames` ⇒ emit the delta (capped by `maxBurst`);
  - for targets without instance counting, the OCR message changes to a *different* message (word-by-word comparison, OCR-noise tolerant) ⇒ emit 1.
- Duplicate suppression: fingerprints (dHash + normalized OCR text) of recent events; the same message re-appearing within `cooldownMs` of when it was last seen is suppressed. `minEventIntervalMs` is a global debounce.
- Old entries expiring (count drops for `releaseFrames`) re-arms instance counting.

## 3. Threading and frame-processing model

| Thread | Owner | Work | Must never |
|---|---|---|---|
| OBS graphics thread | libobs | `FrameSampler::tick`: early-out on most frames; on a sample frame render ROI(s) into small texrenders and `gs_stage_texture`; next tick `gs_stagesurface_map`, `memcpy` ROI rows into a pooled packet, `submit()` | run OCR/detection, block, allocate large buffers |
| KillSense analysis | `AnalysisWorker` | owns `DetectionEngine` + OCR engine (MTA apartment on Windows) | touch Qt or OBS graphics |
| KillSense dispatch | `EventBus` | delivers events to outputs | block the worker |
| Qt UI thread | OBS | dock/dialogs poll `Controller::snapshot()` every 250 ms | wait on the worker |
| Hotkey / websocket threads | libobs / obs-websocket | Test Kill, Reset, Start/Stop via thread-safe Controller API | — |

Frame flow details:
1. **Sampling**: interval `1/analysisFps` (Low Impact 3, Balanced 6, High Accuracy 10). If `worker.canAccept()` is false the sample is *skipped* (counted as "busy skip") — no GPU work is queued.
2. **GPU crop/scale**: one `gs_texrender` per ROI, `gs_ortho(roi)` so the GPU renders only the ROI at the planned size (e.g. a 1440p kill feed is downscaled to 1080p-reference scale).
3. **Asynchronous readback**: stage at tick N, map at tick N+1 (never map a surface in the frame it was staged). Two staging surfaces per region alternate (double buffering).
4. **Bounded queue**: capacity 2, drop-oldest ⇒ OBS is never blocked and results stay fresh. Drops are counted and shown in the dock.
5. **Buffer reuse**: `FramePool` recycles packets and their pixel buffers; images only reallocate when a size grows.
6. **Shutdown**: `obs_remove_tick_callback` (libobs holds `draw_callbacks_mutex` while ticking, so no tick is in flight after it returns) → GPU resources destroyed inside `obs_enter_graphics()` → worker joined → event bus joined. obs-websocket vendor requests are unregistered at `OBS_FRONTEND_EVENT_EXIT`, while obs-websocket is still loaded.

Measured (headless libobs, software GL, see `docs/TESTING.md`): analysis ≈ 5–7 ms per frame for the sample profile, 0 dropped frames at 6 fps. On llvmpipe (CPU-emulated GPU) the graphics-thread cost was ~2 ms per sample; on a real GPU it should be far lower, but **this has not been measured on Windows hardware yet** — the dock shows the live value ("render thread" µs).

## 4. Resolution independence

Profiles store normalized rects (0–1). `planCaptures` converts them for the current source size, applies `aspectOverrides` (e.g. 21:9) and computes `pxPerReference` so templates authored at the profile's reference resolution (default 1080p) match at 720p, 1080p, 1440p or 4K.

## 5. Crash resilience

- Every detector call, event callback, output delivery and OCR call is wrapped in `try/catch`; failures are logged and the pipeline continues.
- Invalid profiles are rejected with precise errors (Profile Manager shows them); invalid `config.json` is backed up to `config.json.invalid` and defaults are used.
- RAII wrappers for OBS references (`src/obs/obs-raii.hpp`), `std::unique_ptr`/`shared_ptr` ownership, no raw owning pointers (Qt parent-owned widgets excepted, as is idiomatic Qt).
- The Controller is shared with UI via `std::weak_ptr`; the dock goes inert after unload.

## 6. Extension points

| Want to add… | Do this |
|---|---|
| A new detector kind | Implement `IDetector`, add a `DetectorKind`, parse it in `profile.cpp::parseDetector`, create it in `createDetector()` |
| ONNX inference (M4) | `IInferenceBackend` with CPU / DirectML / CUDA execution providers behind the existing `onnx` detector kind |
| A new output (localhost server, Streamer.bot) | Implement `IEventOutput` and `bus_.addOutput(...)` in `Controller::init` |
| A new game | Write a JSON profile (docs/PROFILE_SCHEMA.md) — no recompiling |
| Learn This Game UI (M2) | Use `CalibrationRecorder` / `analyzeCandidates` (`core/include/killsense/calibration.hpp`) fed by a low-res full-frame sampler slot |

## 7. Verified OBS APIs (checked against source, not old examples)

Checked against obs-studio `master` (33.0.0-beta3, Sep 2026), the obs-plugintemplate (Dec 2025, pins OBS 31.1.1) and obs-websocket `lib/obs-websocket-api.h` (API v3):

- `obs_add_tick_callback` / `obs_remove_tick_callback` + `obs_enter_graphics` — same pattern as OBS's own `frontend/utility/ScreenshotObj.cpp`.
- `gs_texrender_create`, `gs_texrender_reset`, `gs_texrender_begin_with_color_space(…, GS_CS_SRGB)`, `gs_ortho`, `gs_blend_state_push/pop`, `obs_source_video_render`, `gs_stage_texture`, `gs_stagesurface_map/unmap`.
- `obs_frontend_add_dock_by_id` (OBS ≥ 30.0; replaces the deprecated `obs_frontend_add_dock`).
- `obs_hotkey_register_frontend`, `obs_hotkey_save/load`.
- `obs_source_get_uuid` / `obs_get_source_by_uuid` (OBS ≥ 29.1).
- `os_cpu_usage_info_start/query/destroy`.
- Game/Window Capture `window` setting format `title:class:exe` with `#3A`/`#22` escaping (`libobs/util/windows/window-helpers.c`).
- obs-websocket vendor API: `obs_websocket_register_vendor` in `obs_module_post_load`, `obs_websocket_vendor_emit_event`, `obs_websocket_vendor_register_request`.

**Supported OBS versions**: the Windows build uses the template's pinned OBS 31.1.1 SDK ⇒ OBS **31.x and newer** (libobs refuses modules built for a newer major version). The code itself only needs OBS ≥ 30.0 and was run inside OBS 30.2 on Linux.
