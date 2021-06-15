// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Geometry for a 3D tetrahedron.

#pragma once

#include "PSIGeometry.h"
#include <algorithm>

namespace PSIGeometry {
	namespace CubeTetrahedron {
		extern const std::vector<glm::vec3> positions;
		extern const std::vector<PSI::triangle> faces;
		extern const std::vector<GLuint> indexes;
		extern const std::vector<glm::vec3> normals;
		extern const std::vector<glm::vec2> texcoords;
	}

	namespace Tetrahedron {
		extern const std::vector<glm::vec3> positions;
		extern const std::vector<PSI::triangle> faces;
		extern const std::vector<GLuint> indexes;
		extern const std::vector<glm::vec3> normals;
		extern const std::vector<glm::vec2> texcoords;
	}
}

