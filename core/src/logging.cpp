// SPDX-License-Identifier: GPL-2.0-or-later
#include "killsense/logging.hpp"

#include <atomic>
#include <cstdio>
#include <mutex>
#include <vector>

namespace killsense {

namespace {
std::mutex g_sinkMutex;
LogSink g_sink;
std::atomic<int> g_level{static_cast<int>(LogLevel::Info)};

void defaultSink(LogLevel level, const std::string &msg)
{
	std::fprintf(stderr, "[killsense][%s] %s\n", logLevelName(level), msg.c_str());
}
} // namespace

void setLogSink(LogSink sink)
{
	std::lock_guard<std::mutex> lock(g_sinkMutex);
	g_sink = std::move(sink);
}

void setLogLevel(LogLevel level)
{
	g_level.store(static_cast<int>(level));
}

LogLevel logLevel()
{
	return static_cast<LogLevel>(g_level.load());
}

bool parseLogLevel(std::string_view text, LogLevel &out)
{
	if (text == "error")
		out = LogLevel::Error;
	else if (text == "warning" || text == "warn")
		out = LogLevel::Warning;
	else if (text == "info")
		out = LogLevel::Info;
	else if (text == "debug")
		out = LogLevel::Debug;
	else if (text == "trace")
		out = LogLevel::Trace;
	else
		return false;
	return true;
}

const char *logLevelName(LogLevel level)
{
	switch (level) {
	case LogLevel::Error:
		return "error";
	case LogLevel::Warning:
		return "warning";
	case LogLevel::Info:
		return "info";
	case LogLevel::Debug:
		return "debug";
	case LogLevel::Trace:
		return "trace";
	}
	return "info";
}

void logMessage(LogLevel level, const char *fmt, ...)
{
	if (static_cast<int>(level) > g_level.load())
		return;

	char stackBuf[512];
	va_list args;
	va_start(args, fmt);
	va_list copy;
	va_copy(copy, args);
	const int needed = std::vsnprintf(stackBuf, sizeof(stackBuf), fmt, args);
	va_end(args);

	std::string msg;
	if (needed < 0) {
		msg = fmt;
	} else if (static_cast<size_t>(needed) < sizeof(stackBuf)) {
		msg.assign(stackBuf, static_cast<size_t>(needed));
	} else {
		std::vector<char> heap(static_cast<size_t>(needed) + 1);
		std::vsnprintf(heap.data(), heap.size(), fmt, copy);
		msg.assign(heap.data(), static_cast<size_t>(needed));
	}
	va_end(copy);

	LogSink sink;
	{
		std::lock_guard<std::mutex> lock(g_sinkMutex);
		sink = g_sink;
	}
	try {
		if (sink)
			sink(level, msg);
		else
			defaultSink(level, msg);
	} catch (...) {
		// Logging must never throw into callers.
	}
}

} // namespace killsense
