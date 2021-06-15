// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Color functions.

#pragma once

#include <iostream>
#include <memory>
#include <vector>
#include <stack>
#include <math.h>
#include <time.h>

#include "PSIOpenGL.h"

namespace PSIColor {
	// Color bitmasks.
	enum Bitmask {
		RGB 	= 0x00ffffff,
		RED 	= 0x00ff0000,
		GREEN 	= 0x0000ff00,
		BLUE 	= 0x000000ff,
		ALPHA 	= 0xff000000
	};

	// Convert bitmasked color from int 0..255 to vec4 (r, g, b, a).
	glm::vec4 bitmask_to_vec(GLint color);
	// Convert bitmasked color from int 0..255 to hue, saturation, value, alpha values.
	glm::vec4 bitmask_to_vec_hsv(GLint color);
	// Convert rgb to hsv.
	glm::vec4 rgb_to_hsv(glm::vec3 color);
	// Create array of colors with color_count, rotating hue from 0 .. to 1.0.
	std::vector<glm::vec4> create_hue_rainbow(GLuint color_count);
}
