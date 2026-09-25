// REALM KillSense - per-user template capture ("Capture my name plate").
// Cuts the light (white) text out of a captured HUD region so each player can
// store an image of their OWN name plate. Stored at reference (1080p) scale so
// it matches the profile's other templates.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "killsense/geometry.hpp"
#include "killsense/image.hpp"

#include <string>

namespace killsense {

struct TemplateCrop {
	bool ok = false;
	Image image;   // RGBA, reference scale
	PixelRect box; // crop box in the input region image
	std::string message;
};

// `rgba`: a captured region (RGBA8). `pxPerReference`: captured pixels per
// reference pixel (RegionCapture::pxPerReference).
TemplateCrop cropNameTemplate(const Image &rgba, double pxPerReference);

} // namespace killsense
