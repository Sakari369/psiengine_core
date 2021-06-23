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

	// Vertex ordering.
	//
	//      0
	//      .
	//     / \
	//    /   \
	//   /_____\
	//  2       1

	// Front triangle face.
	const PSI::triangle front = {
		{ glm::vec3(tri_verts[0], -depth/2.0f), 
		  glm::vec3(tri_verts[1], -depth/2.0f), 
		  glm::vec3(tri_verts[2], -depth/2.0f) }
	};

	plog_s("front[0] = %f.%f.%f", front[0].x, front[0].y, front[0].z);
	plog_s("front[1] = %f.%f.%f", front[1].x, front[1].y, front[1].z);
	plog_s("front[2] = %f.%f.%f", front[2].x, front[2].y, front[2].z);

	// Back triangle face.
	const PSI::triangle back = {
		{ glm::vec3(tri_verts[0], depth/2.0f), 
		  glm::vec3(tri_verts[1], depth/2.0f), 
		  glm::vec3(tri_verts[2], depth/2.0f) }
	};

	plog_s("back[0] = %f.%f.%f", back[0].x, back[0].y, back[0].z);
	plog_s("back[1] = %f.%f.%f", back[1].x, back[1].y, back[1].z);
	plog_s("back[2] = %f.%f.%f", back[2].x, back[2].y, back[2].z);

	// Right quad face 0. CW.
	const PSI::triangle right0 = {
		front[0], back[0], back[1]
	};

	plog_s("right0[0] = %f.%f.%f", right0[0].x, right0[0].y, right0[0].z);
	plog_s("right0[1] = %f.%f.%f", right0[1].x, right0[1].y, right0[1].z);
	plog_s("right0[2] = %f.%f.%f", right0[2].x, right0[2].y, right0[2].z);

	// Right quad face 1. CW.
	const PSI::triangle right1 = {
		front[0], back[1], front[1]
	};

	plog_s("right1[0] = %f.%f.%f", right1[0].x, right1[0].y, right1[0].z);
	plog_s("right1[1] = %f.%f.%f", right1[1].x, right1[1].y, right1[1].z);
	plog_s("right1[2] = %f.%f.%f", right1[2].x, right1[2].y, right1[2].z);

	const PSI::triangle left0 = {
		front[0], back[0], back[2]
	};
	const PSI::triangle left1 = {
		front[0], back[2], front[2]
	};

	const PSI::triangle bottom0 = {
		front[2], back[1], front[1]
	};
	const PSI::triangle bottom1 = {
		front[2], back[2], back[1]
	};

	// Do we need actually all the positions ?
	const std::vector<glm::vec3> positions = {
		// Front triangle, cw.
		front[0], front[1], front[2],
		// Back triangle, ccw.
		back[2], back[1], back[0],
		// Right upper, cw.
		right0[0], right0[1], right0[2],
		// Right lower, cw.
		right1[0], right1[1], right1[2],
		// Left upper, ccw.
		left0[2], left0[1], left0[0],
		// Left lower, ccw.
		left1[2], left1[1], left1[0],
		// Bottom upper, ccw.
		bottom0[2], bottom0[1], bottom0[0],
		// Bottom lower, ccw.
		bottom1[2], bottom1[1], bottom1[0]
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
	normal = glm::normalize(glm::cross(front.p[1] - front.p[0], front.p[2] - front.p[0]));
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);

	normal = glm::normalize(glm::cross(back.p[1] - back.p[0], back.p[2] - back.p[0]));
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
