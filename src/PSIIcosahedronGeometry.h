// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Geometry for a 3D icosahedron.

#pragma once

#include "PSIGeometryData.h"
#include "PSIMath.h"
#include <algorithm>

namespace PSIGeometry {
	namespace Icosahedron {
		// Create icosahedron with levels of recursion.
		GeometryDataSharedPtr icosahedron(GLint recursion);
	}
}
