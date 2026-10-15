// KopSkoot - small RAII helpers for libobs reference-counted objects.
// Every obs_*_get_*/create call that returns a reference is wrapped so it is
// released on every code path (including exceptions).
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <obs.h>

#include <memory>
#include <string>

namespace killsense::obsplugin {

struct SourceReleaser {
	void operator()(obs_source_t *s) const { obs_source_release(s); }
};
struct WeakSourceReleaser {
	void operator()(obs_weak_source_t *s) const { obs_weak_source_release(s); }
};
struct DataReleaser {
	void operator()(obs_data_t *d) const { obs_data_release(d); }
};
struct DataArrayReleaser {
	void operator()(obs_data_array_t *a) const { obs_data_array_release(a); }
};
struct BFreeDeleter {
	void operator()(char *p) const { bfree(p); }
};

using SourcePtr = std::unique_ptr<obs_source_t, SourceReleaser>;
using WeakSourcePtr = std::unique_ptr<obs_weak_source_t, WeakSourceReleaser>;
using DataPtr = std::unique_ptr<obs_data_t, DataReleaser>;
using DataArrayPtr = std::unique_ptr<obs_data_array_t, DataArrayReleaser>;
using BStr = std::unique_ptr<char, BFreeDeleter>;

inline std::string toString(const BStr &s)
{
	return s ? std::string(s.get()) : std::string();
}

} // namespace killsense::obsplugin
