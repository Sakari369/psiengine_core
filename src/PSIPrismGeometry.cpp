#include "PSIPrismGeometry.h"

namespace PSIGeometry {
namespace Prism {

// Allright to generate a prism, we first need to:
// Generate 1 triangle, translate that to depth amount.
// Then generate 3 * 2 more triangles, 2 for each side of the prism.
// We already have some code for this in the gdscript generating the miter joins.

shared_data prism(GLfloat radius, GLfloat depth) {
	assert(radius >= 0.0f);
	assert(depth >= 0.0f);

	// Vertices for a single equilateral triangle.
	const GLfloat angle = 0.0f;
	const std::vector<glm::vec2> tri_verts = {
		PSIMath::calc_poly_vertex(3, radius, angle, 0),
		PSIMath::calc_poly_vertex(3, radius, angle, 1),
		PSIMath::calc_poly_vertex(3, radius, angle, 2)
	};

	plog_s("tri_verts[0] = %f.%f", tri_verts[0].x, tri_verts[0].y);
	plog_s("tri_verts[1] = %f.%f", tri_verts[1].x, tri_verts[1].y);
	plog_s("tri_verts[2] = %f.%f", tri_verts[2].x, tri_verts[2].y);

	// Vertex ordering for front and back faces making up the prism.
	//
	//      0
	//      .
	//     / \
	//    /   \
	//   /_____\
	//  2       1
	//
	//  This matches when looking at the object from +Z to .. -Z direction.
	//  Meaning the camera is pointing towards +Z, or towards the viewer.

	// Front triangle face.
	const PSI::triangle f = {
		{ glm::vec3(tri_verts[0], -depth/2.0f), 
		  glm::vec3(tri_verts[1], -depth/2.0f), 
		  glm::vec3(tri_verts[2], -depth/2.0f) }
	};

	// Back triangle face.
	const PSI::triangle b = {
		{ glm::vec3(tri_verts[0], depth/2.0f), 
		  glm::vec3(tri_verts[1], depth/2.0f), 
		  glm::vec3(tri_verts[2], depth/2.0f) }
	};

	// Connect the triangle faces with quads on all three sides of the prism.
	//
	// We should be able to define these just as quads with 4 vertices.

	// Right quad tri 0.
	const PSI::triangle r0 = {
		f[0], b[0], f[1]
	};
	// Right quad tri 1.
	const PSI::triangle r1 = {
		f[1], b[0], b[1]
	};

	// Bottom quad tri 0.
	const PSI::triangle b0 = {
		f[2], b[2], b[1]
	};
	// Bottom quad tri 1.
	const PSI::triangle b1 = {
		f[2], b[1], f[1]
	};

	// Left quad tri 0.
	const PSI::triangle l0 = {
		f[0], b[2], f[2]
	};
	// Left quad tri 1.
	const PSI::triangle l1 = {
		f[0], b[0], b[2]
	};

	// Do we need actually all the positions ?
	const std::vector<glm::vec3> positions = {
		// Front triangle, cw.
		f[0], f[1], f[2],
		// Back triangle, ccw.
		b[2], b[1], b[0],
		// Right upper, cw.
		r0[0], r0[1], r0[2],
		// Right lower, cw.
		r1[0], r1[1], r1[2],
		// Left upper, ccw.
		l0[2], l0[1], l0[0],
		// Left lower, ccw.
		l1[2], l1[1], l1[0],
		// Bottom upper, ccw.
		b0[2], b0[1], b0[0],
		// Bottom lower, ccw.
		b1[2], b1[1], b1[0]
	};

	const std::vector<GLuint> indexes = {
		0, 1, 2,
		3, 4, 5,

		6, 7, 8,
		9, 10, 11,

		12, 13, 14,
		15, 16, 17,

		18, 19, 20,
		21, 22, 23 
	};

	const std::vector<glm::vec2> texcoords = {
		// top right.
		glm::vec2(0.0f, 0.0f),
		glm::vec2(0.0f, 1.0f),
		glm::vec2(1.0f, 1.0f),
		// bottom right.
		glm::vec2(0.0f, 0.0f),
		glm::vec2(0.0f, 1.0f),
		glm::vec2(1.0f, 1.0f),
	};

	shared_data geom = make_shared<PSIGeometryData>();

	glm::vec3 normal;
	normal = glm::normalize(glm::cross(f[1] - f[0], f[2] - f[0]));
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);

	normal = glm::normalize(glm::cross(b[1] - b[0], b[2] - b[0]));
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);

	geom->positions = positions;
	geom->indexes   = indexes;
	geom->texcoords = texcoords;

	return geom;
}
} // namespace Prism
} // namespace PSIGeometry
