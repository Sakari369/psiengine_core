#include "PSIPrismGeometry.h"

namespace PSIGeometry {
namespace Prism {

GeometryDataSharedPtr prism(GLfloat radius, GLfloat depth) {
	assert(radius >= 0.0f);
	assert(depth >= 0.0f);

	// Vertices for a single equilateral triangle.
	const GLfloat angle = 0.0f;
	const std::vector<glm::vec2> tri_verts = {
		PSIMath::calc_poly_vertex(3, radius, angle, 0),
		PSIMath::calc_poly_vertex(3, radius, angle, 1),
		PSIMath::calc_poly_vertex(3, radius, angle, 2)
	};

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

	// Calculate front and back faces so that front and back are translated in equal distance from 0.

	// Front triangle face.
	const PSI::triangle ft = {
		{ glm::vec3(tri_verts[0], -depth/2.0f), 
		  glm::vec3(tri_verts[1], -depth/2.0f), 
		  glm::vec3(tri_verts[2], -depth/2.0f) }
	};

	// Back triangle face.
	const PSI::triangle bt = {
		{ glm::vec3(tri_verts[0], depth/2.0f), 
		  glm::vec3(tri_verts[1], depth/2.0f), 
		  glm::vec3(tri_verts[2], depth/2.0f) }
	};

	// Connect the triangle faces with quads on all three sides of the prism.

	// Right quad, upper and lower triangles.
	const PSI::quad rq = {{
		{ ft[0], bt[0], ft[1] },
		{ ft[1], bt[0], bt[1] }
	}};
	// Bottom quad, upper and lower triangles.
	const PSI::quad bq = {{
		{ ft[2], bt[2], bt[1] },
		{ ft[2], bt[1], ft[1] }
	}};
	// Left quad, upper and lower triangles.
	const PSI::quad lq = {{
		{ ft[0], bt[2], ft[2] },
		{ ft[0], bt[0], bt[2] }
	}};

	GeometryDataSharedPtr geom = PSIGeometryData::create();

	// Form the actual position data sent to the GPU from the face data.
	geom->positions = {
		// Front triangle, cw.
		ft[0], ft[1], ft[2],
		// Back triangle, ccw.
		bt[2], bt[1], bt[0],
		// Right upper, cw.
		rq[0][0], rq[0][1], rq[0][2],
		// Right lower, cw.
		rq[1][0], rq[1][1], rq[1][2],
		// Bottom upper, ccw.
		bq[0][2], bq[0][1], bq[0][0],
		// Bottom lower, ccw.
		bq[1][2], bq[1][1], bq[1][0],
		// Left upper, ccw.
		lq[0][2], lq[0][1], lq[0][0],
		// Left lower, ccw.
		lq[1][2], lq[1][1], lq[1][0],
	};

	// Indexes for each position.
	geom->indexes = {
		0, 1, 2,
		3, 4, 5,
		6, 7, 8,
		9, 10, 11,
		12, 13, 14,
		15, 16, 17,
		18, 19, 20,
		21, 22, 23 
	};

	// Calculate normals from the position data.
	geom->normals.reserve(geom->positions.size());
	glm::vec3 normal;

	// Front.
	normal = glm::triangleNormal(geom->positions[0], geom->positions[1], geom->positions[2]);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);

	// Back.
	normal = glm::triangleNormal(geom->positions[3], geom->positions[4], geom->positions[5]);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);

	// Right.
	normal = glm::triangleNormal(geom->positions[6], geom->positions[7], geom->positions[8]);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	// We can use same normal for second triangle on these quad faces, it is facing in the same direction.
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);

	// Bottom.
	// Skip the position indexes for the second triangle on each quad face as the normal is the same.
	normal = glm::triangleNormal(geom->positions[12], geom->positions[13], geom->positions[14]);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);

	// Left.
	normal = glm::triangleNormal(geom->positions[18], geom->positions[19], geom->positions[20]);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);

	// TODO: these are incorrect.
	geom->texcoords = {
		// Front.
		glm::vec2(0.0f, 0.0f),
		glm::vec2(0.0f, 1.0f),
		glm::vec2(1.0f, 1.0f),
		// Back.
		glm::vec2(0.0f, 0.0f),
		glm::vec2(0.0f, 1.0f),
		glm::vec2(1.0f, 1.0f),
		// Right.
		glm::vec2(0.0f, 0.0f),
		glm::vec2(0.0f, 1.0f),
		glm::vec2(1.0f, 1.0f),
		glm::vec2(0.0f, 0.0f),
		glm::vec2(0.0f, 1.0f),
		glm::vec2(1.0f, 1.0f),
		// Bottom.
		glm::vec2(0.0f, 0.0f),
		glm::vec2(0.0f, 1.0f),
		glm::vec2(1.0f, 1.0f),
		glm::vec2(0.0f, 0.0f),
		glm::vec2(0.0f, 1.0f),
		glm::vec2(1.0f, 1.0f),
		// Left.
		glm::vec2(0.0f, 0.0f),
		glm::vec2(0.0f, 1.0f),
		glm::vec2(1.0f, 1.0f),
		glm::vec2(0.0f, 0.0f),
		glm::vec2(0.0f, 1.0f),
		glm::vec2(1.0f, 1.0f),
	};


	return geom;
}
} // namespace Prism
} // namespace PSIGeometry
