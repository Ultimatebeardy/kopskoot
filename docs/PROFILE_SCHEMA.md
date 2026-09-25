# Game Profile Schema (schemaVersion 1)

A profile tells KillSense **where** to look (regions), **what** to look for (detectors) and **how** to decide (targets). Profiles are JSON files loaded at startup and via Profile Manager → *Reload Profiles*; no recompiling.

Locations (Windows):

| Kind | Folder |
|---|---|
| Built-in (shipped) | `C:\ProgramData\obs-studio\plugins\realm-killsense\data\profiles\` |
| User (yours) | `%APPDATA%\obs-studio\plugin_config\realm-killsense\profiles\` |

A user profile with the same `id` overrides a built-in one. JSON cannot contain comments, but the parser **accepts `//` and `/* */` comments** and ignores any key starting with `_` (e.g. `"_comment"`). Unknown keys produce warnings (visible in Profile Manager); invalid values produce errors and the profile is skipped — OBS never crashes on a bad profile.

## Top level

| Key | Type | Req. | Description |
|---|---|---|---|
| `schemaVersion` | int | ✔ | Must be `1`. |
| `id` | string | ✔ | Unique id: letters, digits, `_ - .` (max 64). |
| `displayName` | string | | Shown in the dock and in events (`game`). Default: `id`. |
| `description`, `version` | string | | Free text. |
| `generic` | bool | | Marks the fallback profile used by Auto Detect when nothing matches. |
| `match` | object | | Non-invasive hints for **Auto Detect** (below). |
| `reference` | `{width,height}` | | Resolution the templates were cut from. Default 1920×1080. |
| `regions` | array | ✔ | 1–8 regions of interest. |
| `aspectOverrides` | array | | Region rects for other aspect ratios. |
| `detectors` | array | ✔ | 1–32 detectors. |
| `targets` | array | ✔ | What to detect and how to decide. |

### `match`
Auto Detect only reads the window a **Game Capture / Window Capture** source is configured for (OBS's own settings) and the source name. No processes are inspected.

| Key | Example | Match rule |
|---|---|---|
| `processNames` | `["Game.exe"]` | exact, case-insensitive |
| `windowTitleContains` | `["My Game"]` | substring of the window title |
| `sourceNameContains` | `["My Game"]` | substring of the OBS source name |

### `regions[]`
| Key | Type | Description |
|---|---|---|
| `id` | string | Referenced by detectors. |
| `rect` | `{x,y,w,h}` or `[x,y,w,h]` | **Normalized** 0–1 of the source frame. `x+w ≤ 1`, `y+h ≤ 1`. |
| `description` | string | Optional. |

Tip: normalized = pixel / frame size. A kill feed at pixels (1344, 65) size 557×216 on 1920×1080 ⇒ `{ "x": 0.70, "y": 0.06, "w": 0.29, "h": 0.20 }`. Keep regions tight: smaller regions are faster and more accurate.

### `aspectOverrides[]`
| Key | Type | Description |
|---|---|---|
| `aspect` | string | `"21:9"`, `"32:9"` or a number like `"2.37"`. Used when the source aspect is within 2%. |
| `regions` | object | `{ "regionId": rect }` replacements. |

## `detectors[]`

Common keys for every type: `id`, `type`, `region`, `enabled` (default true) and
`vetoWhenActive` (default false) — when true, the detector raises a **veto**
whenever it is active, e.g. a full-width red "you are dead / spectating" bar
that must block kills. Give it weight 0 in the targets it should gate.

**`{gamertag}` placeholder.** OCR `keywords`, `patterns` and `negativeWords`
may contain `{gamertag}`; it is replaced by the gamertag the streamer enters in
Settings (regex-escaped inside `patterns`). If no gamertag is set, those
detectors are switched off and the dock asks for one — so one profile works
for every player.
Common keys: `id` (string, unique), `type`, `region` (region id), `enabled` (bool, default true).

### `type: "template"` — icon / image match
| Key | Default | Description |
|---|---|---|
| `file` | – | PNG/JPG/BMP path **relative** to the profile folder (or its parent `data` folder, e.g. `templates/x.png`). `..` is not allowed. Cut it at the `reference` resolution, tightly around the icon. |
| `threshold` | 0.80 | Normalized cross-correlation needed for a match (0.1–1). 0.80–0.90 is typical. |
| `scales` | `[1.0]` | Extra scale factors to try (0.25–4, ≤ 8 entries), e.g. `[0.9, 1.0, 1.1]` if the HUD scales. |
| `maxInstances` | 5 | Max distinct matches reported (used for rapid-kill counting). |
| `userTemplate` | – | Instead of (or in addition to) `file`: a **per-player** image name, e.g. `"own-name-plate"`. Each streamer captures it once with the dock button **Capture my name plate**; it is stored at `plugin_config/realm-killsense/user-templates/<profileId>/<name>.png` (never uploaded). If it has not been captured the detector reports unavailable and the dock shows a setup hint. |

### `type: "ocr"` — text
| Key | Default | Description |
|---|---|---|
| `keywords` | `[]` | Phrases; fuzzy-matched word by word (OCR mistakes tolerated). Keywords ≤ 5 letters must match exactly. |
| `patterns` | `[]` | ECMAScript regular expressions, always case-insensitive. Tested on raw and normalized text. |
| `negativeWords` | `[]` | If any is present the detector raises a **veto** (e.g. "ELIMINATED BY" means *you* died). |
| `fuzzy` | 0.80 | Minimum similarity for keywords (0.5–1). |
| `upscale` | 2.0 | Upscale before OCR (1–4); helps small HUD text. |
| `refreshMs` | 1000 | Re-run OCR at least this often even if the region looks unchanged. |
| `role` | – | Kill-feed mode: `"actor"` = the keyword is the **killer** (another name follows it on its feed line), `"victim"` = the keyword is the last name on its line. Each matching line is one instance. Use with `"keywords": ["{gamertag}"]`. |

Needs an OCR backend (Windows.Media.Ocr, with a Windows OCR language pack). Without one the detector is reported *unavailable* and excluded from fusion.

### `type: "color"` — colour / layout cue
| Key | Default | Description |
|---|---|---|
| `hsv` | – (req.) | `{hMin,hMax}` degrees 0–360 (hMin > hMax wraps around red), `{sMin,sMax,vMin,vMax}` 0–1. |
| `minFraction` | 0.01 | Fraction of region pixels that must match to be active. |
| `maxFraction` | 1.0 | Above this the signal is 0 (e.g. a full red damage flash). |

### `type: "hud_change"` — visual change
| Key | Default | Description |
|---|---|---|
| `threshold` | 10 | Mean absolute difference (0–255) between consecutive samples that counts as a change. |
| `holdMs` | 1200 | Evidence decays linearly for this long after a change. |

### `type: "onnx"` — planned (Milestone 4)
`model`, `threshold`. Parsed and reported *unavailable* until the inference backend exists.

## `targets[]`
A target is one thing to detect (usually `kill`; the sample profile also has `death`). Each target has its own fusion and temporal tracker.

| Key | Default | Description |
|---|---|---|
| `id` | event id | Unique target id. |
| `event` | `"kill"` | `kill, headshot, double_kill, triple_kill, multi_kill, assist, enemy_down, death, vehicle_destroyed, boss_kill`. |
| `signals` | – (req.) | `[{detector, weight, countInstances?, veto?}]`. |
| `threshold` | 0.80 | Confidence needed (0.05–1). |
| `minSignals` | 2 | Number of **active** signals needed. Keep ≥ 2 so no single cue (e.g. OCR alone) proves a kill. |
| `requireAny` | `[]` | At least one of these detectors must be active (e.g. icon or text — not just colour + change). |
| `requireAll` | `[]` | Every one of these detectors must be **available and active**. A missing template or OCR engine therefore blocks the target instead of being ignored. |
| `temporal` | see below | Timing rules. |
| `rules` | `[]` | Reclassify the event (below). |

### `signals[]`
| Key | Default | Description |
|---|---|---|
| `detector` | – | Detector id. |
| `weight` | 0 | Relative importance. Confidence = Σ(weight × score) / Σ(weight of available signals). |
| `countInstances` | false | Use this detector's instance count for rapid consecutive kills. |
| `veto` | true | Honour the detector's veto (OCR negative words, `vetoWhenActive`). |

### `temporal`
| Key | Default | Description |
|---|---|---|
| `confirmFrames` | 2 | Consecutive passing samples before an event fires (at 6 fps: ~330 ms). |
| `releaseFrames` | 3 | Consecutive failing samples before the message is considered gone. |
| `releaseRatio` | 0.75 | A sample only "fails" if confidence < threshold × ratio (flicker tolerance). |
| `cooldownMs` | 1500 | The *same* message (same OCR text, or same look when no text) re-appearing within this gap is not a new kill. |
| `minEventIntervalMs` | 150 | Global debounce between two events of this target. |
| `maxBurst` | 4 | Max events from one instance-count jump. |
| `maxHamming` | 8 | dHash distance treated as "same look" (0–64). |
| `textSimilarity` | 0.80 | Per-word similarity treated as "same message". `0` = text changes never start a new event (e.g. a respawn countdown). Only OCR text read in the target's first-signal region counts as "the message". |
| `recentCapacity` | 16 | Remembered fingerprints. |

### `rules[]`
Evaluated in order when a target fires; the first matching rule sets the event type. Otherwise the target's `event` is used — KillSense never guesses.

| Key | Description |
|---|---|
| `event` | Event type to report. |
| `textPatterns` | Regexes tested on OCR text of the target's OCR signals. |
| `detectorsActive` | Detector ids that must all be active. |

## Creating a profile for a new game (manual, Milestone 1)

1. Take a 1920×1080 screenshot (OBS: right-click the source → *Screenshot*) at the moment a kill indicator is visible.
2. Measure the HUD area in pixels; convert to normalized (divide by 1920/1080). Add a margin of a few pixels.
3. Crop the kill icon tightly and save it as PNG next to your profile (e.g. `templates/mygame-kill.png`).
4. Copy `sample-killfeed.json` to your user profile folder, change `id`, `displayName`, `match`, the region rect, template file, keywords/negative words.
5. Profile Manager → *Reload Profiles*; select it; open **Debug View** and do a kill: check the region is right, the template box appears, OCR text is read, and the target shows PASS.
6. Tune `threshold`, weights and `temporal` until one kill = one event. Use the replay tool with recorded screenshots to test changes offline (docs/TESTING.md).

Learn This Game (Milestone 2) will automate steps 1–4.

## Shipped profiles

- `generic.json` — heuristic fallback: OCR for common kill phrases in the usual kill-feed and banner areas, plus HUD change. Requires OCR; lower accuracy; other players' kill-feed lines can cause false positives.
- `sample-killfeed.json` — reference profile for the simulated kill feed in `data/test-assets` and `tests/data`. Demonstrates template + OCR + colour + change fusion, instance counting, negative words, a `death` target and a headshot rule.
