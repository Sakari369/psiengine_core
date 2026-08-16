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
		// The half-unit extent is the same convention the cube uses, and it is
		// load bearing rather than tidy: a shader that wants a vertex in object
		// space can write `axis * 0.5` and get one, exactly as it writes the
		// same thing for a cube's face centre. plasma_cube's streams rely on
		// that -- they spawn from the corners of this shape using the code they
		// used for the cube's faces, unchanged.
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
		//
		// Shaders selecting a per-face texture must agree with this, and so must
		// any script naming those textures. octa_faces.metal spells the same
		// mapping out in its branches.
		GLint octant_index(GLint sx, GLint sy, GLint sz);
	}
}
