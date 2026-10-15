// KopSkoot - minimal logging facade.
// The core library never depends on OBS. The plugin installs a sink that
// forwards to blog(); tests and the replay tool use the default stderr sink.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdarg>
#include <functional>
#include <string>
#include <string_view>

namespace killsense {

enum class LogLevel { Error = 0, Warning = 1, Info = 2, Debug = 3, Trace = 4 };

using LogSink = std::function<void(LogLevel, const std::string &)>;

// Replace the global sink (thread-safe). Pass nullptr to restore stderr.
void setLogSink(LogSink sink);
// Messages above this level are discarded before formatting.
void setLogLevel(LogLevel level);
LogLevel logLevel();

bool parseLogLevel(std::string_view text, LogLevel &out);
const char *logLevelName(LogLevel level);

#if defined(__GNUC__) || defined(__clang__)
#define KS_PRINTF_FMT(a, b) __attribute__((format(printf, a, b)))
#else
#define KS_PRINTF_FMT(a, b)
#endif

void logMessage(LogLevel level, const char *fmt, ...) KS_PRINTF_FMT(2, 3);

} // namespace killsense

#define KS_LOG_ERROR(...) ::killsense::logMessage(::killsense::LogLevel::Error, __VA_ARGS__)
#define KS_LOG_WARN(...) ::killsense::logMessage(::killsense::LogLevel::Warning, __VA_ARGS__)
#define KS_LOG_INFO(...) ::killsense::logMessage(::killsense::LogLevel::Info, __VA_ARGS__)
#define KS_LOG_DEBUG(...)                                                  \
	do {                                                               \
		if (::killsense::logLevel() >= ::killsense::LogLevel::Debug) \
			::killsense::logMessage(::killsense::LogLevel::Debug, __VA_ARGS__); \
	} while (0)
#define KS_LOG_TRACE(...)                                                  \
	do {                                                               \
		if (::killsense::logLevel() >= ::killsense::LogLevel::Trace) \
			::killsense::logMessage(::killsense::LogLevel::Trace, __VA_ARGS__); \
	} while (0)
