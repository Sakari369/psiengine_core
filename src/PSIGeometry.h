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

namespace PSIGeometry {
	// Shared geometry data type.
	using SharedGeometryData = shared_ptr<PSIGeometryData>;

	// Functions for creating geometry primitive data.
	SharedGeometryData cube();
	SharedGeometryData cube_tetrahedron();
	SharedGeometryData tetrahedron();
	SharedGeometryData cube_inverted();
	SharedGeometryData cuboid(GLfloat width, GLfloat height, GLfloat depth);
	SharedGeometryData plane(GLint rows, GLboolean repeat_texture);
	SharedGeometryData icosahedron(GLint recursion);

	std::array<glm::vec3, 4> quad(glm::vec2 origin, glm::vec2 radius);
	std::vector<glm::vec3> create_poly(GLint num_points, GLfloat angle_offset, GLfloat radius);

	// Add default opengl buffer values to shared geometry data.
	void add_buffer_defaults(const SharedGeometryData &geom);
} // PSIGeometry
