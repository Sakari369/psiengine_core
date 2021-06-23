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
	const GLfloat angle = 150.0f;
	const std::vector<glm::vec2> tri_verts = {
		PSIMath::calc_poly_vertex(3, radius, angle, 0),
		PSIMath::calc_poly_vertex(3, radius, angle, 1),
		PSIMath::calc_poly_vertex(3, radius, angle, 2)
	};

	// Front face.
	const PSI::triangle front = {
		{ glm::vec3(tri_verts[0], 0.0f), 
		  glm::vec3(tri_verts[1], 0.0f), 
		  glm::vec3(tri_verts[2], 0.0f) }
	};

	// Back face.
	const PSI::triangle back = {
		{ glm::vec3(tri_verts[0], depth), 
		  glm::vec3(tri_verts[1], depth), 
		  glm::vec3(tri_verts[2], depth) }
	};

	/*
	# Calculate remaining faces by connecting the vertices between front and back face.
	var verts := PoolVector3Array([
		# Front upper face, CW.
		front[2], back[2], back[1],
		front[2], back[1], front[1],
		# Front lower face, CW.
		front[1], back[1], back[0],
		front[1], back[0], front[0],
		# Back upper face, CCW.
		front[4], back[5], back[4],
		front[4], front[5], back[5],
		# Back lower face, CCW.
		front[5], front[3], back[5],
		back[5], front[3], back[3]
	])
	*/

	const std::vector<glm::vec3> positions = {
		// Front CW.
		//      0
		//      .
		//     / \
		//    /   \
		//   /_____\
		//  2       1
		front.p[0], front.p[1], front.p[2],
		// Back CCW.
		back.p[2], back.p[1], back.p[0],

		// Front upper, CW.
		//front.p[2], back.p[2], back.p[1],
	};

	shared_data geom = make_shared<PSIGeometryData>();
	if (geom == nullptr) {
		return nullptr;
	}

	const std::vector<GLuint> indexes = {
		0, 1, 2,
		3, 4, 5
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
