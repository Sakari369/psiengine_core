// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Geometry for a 3D icosahedron.

#pragma once

#include "PSIGeometryData.h"
#include "PSIMath.h"
#include <algorithm>

namespace PSIGeometry {
	using shared_data = shared_ptr<PSIGeometryData>;

	namespace Icosahedron {
		// Create icosahedron with levels of recursion.
		shared_data icosahedron(GLint recursion);
	}
}
