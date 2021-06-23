#include "PSICuboidGeometry.h"
#include "PSICubeGeometry.h"

namespace PSIGeometry {
namespace Cuboid {

shared_data cuboid(GLfloat width, GLfloat height, GLfloat depth) {
	assert(width >= 0.0f);
	assert(height >= 0.0f);
	assert(depth >= 0.0f);

	const glm::vec3 vec_m = {
		width, height, depth
	};

	const std::vector<glm::vec3> cv {
		// Back face vertexes.
		// 0                   1
		//  ___________________
		// /                   \
		// |                   |
		// |                   |
		// |                   |
		// \___________________/
		// 2                   3
		glm::vec3( 0.5f, -0.5f, -0.5f) * vec_m, // 0
		glm::vec3( 0.5f,  0.5f, -0.5f) * vec_m, // 1
		glm::vec3(-0.5f,  0.5f, -0.5f) * vec_m, // 2
		glm::vec3(-0.5f, -0.5f, -0.5f) * vec_m, // 3

		// Front face vertexes.
		glm::vec3( 0.5f, -0.5f,  0.5f) * vec_m, // 4
		glm::vec3( 0.5f,  0.5f,  0.5f) * vec_m, // 5
		glm::vec3(-0.5f,  0.5f,  0.5f) * vec_m, // 6
		glm::vec3(-0.5f, -0.5f,  0.5f) * vec_m, // 7
	};

	const std::vector<PSI::quad_buf> cf = {
		{{ cv[6], cv[7], cv[4], cv[5] }}, // front, ccw.
		{{ cv[2], cv[1], cv[0], cv[3] }}, // back, cw.
		{{ cv[5], cv[4], cv[0], cv[1] }}, // right, ccw.
		{{ cv[6], cv[2], cv[3], cv[7] }}, // left, cw.
		{{ cv[3], cv[0], cv[4], cv[7] }}, // bottom, cw.
		{{ cv[2], cv[6], cv[5], cv[1] }}  // top, ccw.
	};

	const std::vector<glm::vec3> positions = {
		cf[0].p[0], cf[0].p[1], cf[0].p[2], cf[0].p[3], // front.
		cf[1].p[0], cf[1].p[1], cf[1].p[2], cf[1].p[3], // back.
		cf[2].p[0], cf[2].p[1], cf[2].p[2], cf[2].p[3], // right.
		cf[3].p[0], cf[3].p[1], cf[3].p[2], cf[3].p[3], // left.
		cf[4].p[0], cf[4].p[1], cf[4].p[2], cf[4].p[3], // bottom.
		cf[5].p[0], cf[5].p[1], cf[5].p[2], cf[5].p[3], // top.
	};

	shared_data geom = make_shared<PSIGeometryData>();
	if (geom == nullptr) {
		return nullptr;
	}

	geom->positions = positions;
	geom->normals   = PSIGeometry::Cube::normals;
	geom->texcoords = PSIGeometry::Cube::texcoords;
	geom->indexes   = PSIGeometry::Cube::indexes;

	return geom;
}
} // namespace Cuboid
} // namespace PSIGeometry
