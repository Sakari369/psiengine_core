// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Non-uniform cube geometry.

#pragma once

#include "PSIGeometryData.h"
#include "PSIMath.h"
#include <algorithm>

namespace PSIGeometry {
	using shared_data = shared_ptr<PSIGeometryData>;

	namespace Prism {
		// Calculate a prism shape, with given radius and depth in z-axis direction.
		shared_data prism(GLfloat radius, GLfloat depth);
	}
}
