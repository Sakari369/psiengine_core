// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Geometry for a cube.

#pragma once

#include "PSIGeometry.h"
#include <algorithm>

namespace PSIGeometry {
	namespace Cube {
		extern const std::vector<glm::vec3> positions;
		extern const std::vector<GLuint> indexes;
		extern const std::vector<GLuint> indexes_ccw;
		extern const std::vector<glm::vec3> normals;
		extern const std::vector<glm::vec2> texcoords;
	}
}
