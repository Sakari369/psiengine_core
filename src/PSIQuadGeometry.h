// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Geometry for a simple 3D quad.

#pragma once

#include "PSIGeometry.h"
#include <algorithm>

namespace PSIGeometry {
	namespace Quad {
		// Create quad with origin and radius.
		std::array<glm::vec3, 4> quad(glm::vec2 origin, glm::vec2 radius);
		// Get static quad indexes.
		std::array<GLuint, 6> get_quad_indexes(GLint quad_index);
	}
}
