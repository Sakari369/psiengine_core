// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Encompassing namespace for defining all non-trivial geometry data.

#pragma once

#include <iostream>
#include <vector>
#include <math.h>

#include "PSIVideo.h"
#include "PSIGlobals.h"
#include "PSIFileUtils.h"
#include "PSIOpenGL.h"
#include "PSIMath.h"

#include "PSIGeometryData.h"
#include "PSICubeGeometry.h"
#include "PSICuboidGeometry.h"
#include "PSIPlaneGeometry.h"
#include "PSIQuadGeometry.h"
#include "PSIIcosahedronGeometry.h"
#include "PSITetrahedronGeometry.h"
#include "PSIPrismGeometry.h"

namespace PSIGeometry {
	// Functions for creating geometry primitive data.
	GeometryDataSharedPtr cube();
	GeometryDataSharedPtr cube_tetrahedron();
	GeometryDataSharedPtr tetrahedron();
	GeometryDataSharedPtr prism(GLfloat radius, GLfloat depth);
	GeometryDataSharedPtr cube_inverted();
	GeometryDataSharedPtr cuboid(GLfloat width, GLfloat height, GLfloat depth);
	GeometryDataSharedPtr plane(GLint rows, GLboolean repeat_texture);
	GeometryDataSharedPtr icosahedron(GLint recursion);

	std::array<glm::vec3, 4> quad(glm::vec2 origin, glm::vec2 radius);
	std::vector<glm::vec3> create_poly(GLint num_points, GLfloat angle_offset, GLfloat radius);

	// Add default opengl buffer values to shared geometry data.
	void add_buffer_defaults(const GeometryDataSharedPtr &geom);
} // PSIGeometry
