// REALM KillSense - core version information
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

namespace killsense {

// Version of the event JSON schema emitted by KillSense (see docs/EVENT_SCHEMA.md).
inline constexpr int kEventSchemaVersion = 1;
// Version of the game profile JSON schema (see docs/PROFILE_SCHEMA.md).
inline constexpr int kProfileSchemaVersion = 1;
// Version of the plugin configuration file schema (config.json).
inline constexpr int kConfigSchemaVersion = 1;

inline constexpr const char *kCoreVersion = "0.2.0";
inline constexpr const char *kVendorName = "RealmKillSense";

} // namespace killsense
