// KopSkoot - shared Qt UI helpers.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <obs-module.h>

#include <QString>

#include <string>

namespace killsense::obsplugin {

// Localized text from data/locale/*.ini
inline QString T(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

inline QString Q(const std::string &s)
{
	return QString::fromUtf8(s.c_str(), static_cast<int>(s.size()));
}

inline std::string S(const QString &s)
{
	return s.toUtf8().toStdString();
}

} // namespace killsense::obsplugin
