# KillSense Event Schema (schemaVersion 1)

Every event produced by KillSense — vision detections, Test Kill Event, Session Reset — uses the same JSON document. It is serialized by `core/src/event.cpp` (`toJson`) and is what obs-websocket clients receive as `eventData`.

```json
{
  "schemaVersion": 1,
  "event": "kill",
  "game": "Sample Kill Feed (test profile)",
  "profile": "sample-killfeed",
  "target": "kill",
  "confidence": 0.94,
  "sessionKills": 17,
  "killStreak": 4,
  "bestStreak": 6,
  "sessionDeaths": 2,
  "sequence": 21,
  "timestamp": "2026-09-25T04:42:26.512Z",
  "timestampMs": 1790311346512,
  "detector": "vision",
  "test": false,
  "reason": "confirmed over 2 frame(s)"
}
```

| Field | Type | Meaning |
|---|---|---|
| `schemaVersion` | int | Always `1` for this format. Additive fields may appear later without a version bump; breaking changes bump it. |
| `event` | string | Event id (table below). |
| `game` | string | Profile display name (`"unknown"` if none). |
| `profile` | string | Profile id. |
| `target` | string | Profile target that fired (`"test"` for Test Kill Event). |
| `confidence` | number | Fused confidence 0–1 (3 decimals). |
| `sessionKills` | int | Kills this session **after** this event. |
| `killStreak` | int | Kills since the last death/reset **after** this event. |
| `bestStreak` | int | Best streak this session. |
| `sessionDeaths` | int | Deaths this session. |
| `sequence` | int | Increments per event in the session (0 for SessionReset). |
| `timestamp` | string | ISO-8601 UTC with milliseconds. |
| `timestampMs` | int | Unix epoch milliseconds. |
| `detector` | string | `vision` (live detection), `test` (Test Kill Event), `manual` (reset), `replay` (offline tool). |
| `test` | bool | `true` for Test Kill Event. Test kills **do** increment counters so automations can be tested end to end. |
| `reason` | string | Short decision reason (optional). |
| `evidence` | array | Optional (JSONL/debug only): `{detector, kind, weight, score, contribution, active}` per signal. Not sent over obs-websocket. |

## Event types

| `event` id | obs-websocket `eventType` | Counts as kill | Emitted in Milestone 1 |
|---|---|---|---|
| `kill` | `Kill` | yes | yes |
| `headshot` | `Headshot` | yes | yes, when a profile rule proves it (e.g. OCR "HEADSHOT") |
| `double_kill` | `DoubleKill` | no (announcement) | only via profile rules |
| `triple_kill` | `TripleKill` | no | only via profile rules |
| `multi_kill` | `MultiKill` | no | only via profile rules |
| `assist` | `Assist` | no | via a profile target |
| `enemy_down` | `EnemyDown` | no (knock ≠ kill) | via rules/targets |
| `death` | `Death` | no — ends the streak | via a profile target (sample profile has one) |
| `vehicle_destroyed` | `VehicleDestroyed` | no | via a profile target |
| `boss_kill` | `BossKill` | yes | via a profile target |
| `session_reset` | `SessionReset` | — resets counters | yes (Reset Session) |

KillSense never *guesses* a richer type. Double/Triple/Multi kill are announcements the game shows; they are only emitted when a profile rule sees them, and they never add extra kills (the individual kills are counted separately).

## obs-websocket (v5) integration

- Vendor name: **`RealmKillSense`** (registered in `obs_module_post_load`).
- Events arrive as `VendorEvent` (op 5) with `eventData = { vendorName: "RealmKillSense", eventType: "Kill", eventData: { …schema above… } }`. Subscribe to the **Vendors** event category (bit `1 << 9`; included in the default "All").
- Vendor requests (`CallVendorRequest`, `vendorName: "RealmKillSense"`):

| requestType | requestData | responseData |
|---|---|---|
| `GetStatus` | – | detecting, detectorState, source, profile, game, sessionKills, killStreak, bestStreak, sessionDeaths, kdRatio, setupHint, analysisFps, avgProcessingMs, droppedFrames, ocrEngine, ocrAvailable, lastEvent |
| `TestKill` | – | `{ok}` and a `Kill` event with `test: true` |
| `ResetSession` | – | `{ok}` and a `SessionReset` event |
| `StartDetection` | `{sourceName?, profileId?}` | `{ok, error?}` |
| `StopDetection` | – | `{ok}` |

`tools/websocket/killsense_ws_test.py` is a complete, working client example.

### Streamer.bot

Streamer.bot connects to OBS through obs-websocket and documents an OBS **Vendor Event** (https://docs.streamer.bot/api/websocket/events/obs/vendor-event, triggers: https://docs.streamer.bot/api/triggers/obs-studio). Filter on vendor `RealmKillSense` and event type `Kill`, then read `killStreak`, `sessionKills`, etc. from the event data. This path has not yet been tested with a live Streamer.bot install. A dedicated Streamer.bot integration (pre-built actions/import string) is planned for Milestone 3; the schema above is already stable for it.

## JSON Lines file output

Optional (Settings → Event Outputs). One compact JSON object per line, same schema, appended to the chosen file. Disabled by default.

## Building an overlay (Browser Source)

Overlays do not need to be part of the plugin. A Browser Source page can connect
to OBS's own WebSocket server (`ws://127.0.0.1:4455`), identify with
`eventSubscriptions: 512` (Vendors), call `CallVendorRequest`
`{vendorName: "RealmKillSense", requestType: "GetStatus"}` once for the current
counters and then react to `VendorEvent` messages whose `vendorName` is
`RealmKillSense` (`eventType` `Kill`, `Death`, `SessionReset`, ...; the event
data carries `sessionKills`, `sessionDeaths`, `killStreak`). The simplest
counter needs no overlay at all: Settings → Event Outputs → *On-screen counter*.
