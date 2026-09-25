// REALM KillSense - UTF-8 aware filesystem helpers.
// OBS hands out UTF-8 paths. On Windows std::string paths would otherwise be
// interpreted in the ANSI code page, so every file access goes through here.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <filesystem>
#include <string>

namespace killsense {

std::filesystem::path pathFromUtf8(const std::string &utf8);
std::string pathToUtf8(const std::filesystem::path &path);

bool readTextFile(const std::filesystem::path &path, std::string &out, size_t maxBytes = 8u * 1024u * 1024u);
// Writes via a temporary file + rename so a crash never leaves a truncated file.
bool writeTextFileAtomic(const std::filesystem::path &path, const std::string &content);

} // namespace killsense
