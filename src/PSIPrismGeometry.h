// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Non-uniform cube geometry.

#pragma once

#include "PSIGeometryData.h"
#include "PSIMath.h"
#include <algorithm>

namespace PSIGeometry {
	namespace Prism {
		// Calculate a prism shape, with given radius and depth in z-axis direction.
		GeometryDataSharedPtr prism(GLfloat radius, GLfloat depth);
	}
}
