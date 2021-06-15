// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// 3D Sphere geometry.

#pragma once

#include "PSIMath.h"
#include "PSIOpenGL.h"
#include "PSIGeometryData.h"
#include "PSIGLUtils.h"

#include <algorithm>

namespace PSIGeometry {
	// Register this directly in the PSIGeometry namespace
	// so we can call it by PSIGeometry::sphere()
	shared_ptr<PSIGeometryData> sphere(GLfloat radius, GLint widthSegments, GLint heightSegments);
					   //GLfloat phiStart, GLfloat phiLength, GLfloat thetaStart, GLfloat thetaLength);
}