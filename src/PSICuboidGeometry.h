// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Non-uniform cube geometry.

#pragma once

#include "PSIGeometryData.h"
#include "PSIMath.h"
#include <algorithm>

namespace PSIGeometry {
	using shared_data = shared_ptr<PSIGeometryData>;

	namespace Cuboid {
		shared_data cuboid(GLfloat width, GLfloat height, GLfloat depth);
	}
}
