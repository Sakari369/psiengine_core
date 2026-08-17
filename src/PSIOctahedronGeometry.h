// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Geometry for a 3D octahedron -- the eight-sided diamond.

#pragma once

#include "PSIGeometry.h"

namespace PSIGeometry {
	namespace Octahedron {
		// Six vertexes on the axes at +/-0.5, eight triangular faces, one per
		// octant.
		//
		//               +Y
		//               /\
		//              /  \
		//        -X   /    \   +Z
		//          \ /      \ /
		//           X--------X
		//          / \      / \
		//        -Z   \    /   +X
		//              \  /
		//               \/
		//               -Y
		//
		// Faces are flat shaded, so no vertex is shared between two of them:
		// 8 faces x 3 vertexes = 24, the same count as the cube's 6 x 4.
		GeometryDataSharedPtr octahedron();

		// The octant a face belongs to, as an index 0..7, from the signs of its
		// normal: bit 0 is +X, bit 1 is +Y, bit 2 is +Z.
		GLint octant_index(GLint sx, GLint sy, GLint sz);
	}
}
