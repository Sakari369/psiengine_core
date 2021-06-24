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

	const std::vector<glm::vec3> positions = {
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

	shared_data geom = make_shared<PSIGeometryData>();
	
	// Front.
	glm::vec3 normal;
	//normal = PSIMath::calc_tri_normal(ft);
	normal = glm::triangleNormal(ft[0], ft[1], ft[2]);
	print_vec(ft[0]);
	print_vec(ft[1]);
	print_vec_named("ft_norm", normal);

	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);

	// Back.
	normal = glm::triangleNormal(bt[0], bt[1], bt[2]);
	print_vec(bt[0]);
	print_vec(bt[1]);
	print_vec_named("bt_norm", normal);

	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);

	// Right.
	normal = glm::triangleNormal(rq[0][0], rq[0][1], rq[0][2]);
	print_vec_named("rq0", normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);

	normal = glm::triangleNormal(rq[1][0], rq[1][1], rq[1][2]);
	print_vec_named("rq1", normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);

	// Bottom.
	normal = glm::triangleNormal(bq[0][0], bq[0][1], bq[0][2]);
	print_vec_named("bq0", normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);

	normal = glm::triangleNormal(bq[1][0], bq[1][1], bq[1][2]);
	print_vec_named("bq1", normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);

	// Left.
	normal = glm::triangleNormal(lq[0][0], lq[0][1], lq[0][2]);
	print_vec_named("lq0", normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);

	normal = glm::triangleNormal(lq[1][0], lq[1][1], lq[1][2]);
	print_vec_named("lq0", normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);

	// TODO: these are incorrect.
	const std::vector<glm::vec2> texcoords = {
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
		// Bottom.
		glm::vec2(0.0f, 0.0f),
		glm::vec2(0.0f, 1.0f),
		glm::vec2(1.0f, 1.0f),
		// Left.
		glm::vec2(0.0f, 0.0f),
		glm::vec2(0.0f, 1.0f),
		glm::vec2(1.0f, 1.0f),
	};

	geom->positions = positions;
	geom->indexes   = indexes;
	geom->texcoords = texcoords;

	return geom;
}
} // namespace Prism
} // namespace PSIGeometry