// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Geometry for a simple 3D plane.

#pragma once

#include "PSIGeometryData.h"
#include <algorithm>

namespace PSIGeometry {
	namespace Plane {
		// Create a grid of lines.
		std::vector<glm::vec3> line_grid(GLint rows);
		// Create a plane with uniform rows.
		shared_ptr<PSIGeometryData> uniform_plane(GLint rows, GLboolean repeat_texture);
	}
}
