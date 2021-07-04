#include "PSIIcosahedronGeometry.h"

namespace PSIGeometry {
namespace Icosahedron {

GeometryDataSharedPtr icosahedron(GLint recursion) {
	GeometryDataSharedPtr geom = PSIGeometryData::create();
	if (geom == nullptr) {
		return nullptr;
	}

	GLint vertex_count = 12;
	GLfloat len = GOLDEN_RATIO;

	// Create the 12 vertices of a icosahedron.

	// Corners of three orthogonal rectangles, with
	// side length of len, form the icosahedron 12 points.
	geom->positions.reserve(vertex_count);

	geom->positions.push_back(glm::normalize(glm::vec3(-1,  len,  0)));
	geom->positions.push_back(glm::normalize(glm::vec3( 1,  len,  0)));
	geom->positions.push_back(glm::normalize(glm::vec3(-1, -len,  0)));
	geom->positions.push_back(glm::normalize(glm::vec3( 1, -len,  0)));

	geom->positions.push_back(glm::normalize(glm::vec3( 0, -1,  len)));
	geom->positions.push_back(glm::normalize(glm::vec3( 0,  1,  len)));
	geom->positions.push_back(glm::normalize(glm::vec3( 0, -1, -len)));
	geom->positions.push_back(glm::normalize(glm::vec3( 0,  1, -len)));

	geom->positions.push_back(glm::normalize(glm::vec3( len, 0, -1)));
	geom->positions.push_back(glm::normalize(glm::vec3( len, 0,  1)));
	geom->positions.push_back(glm::normalize(glm::vec3(-len, 0, -1)));
	geom->positions.push_back(glm::normalize(glm::vec3(-len, 0,  1)));

	// create 20 triangle faces of the icosahedron.
	std::vector<PSI::tri_indexes> tri_indexes;

	// 5 tri_indexes around center point
	// in counter clockwise order.
	tri_indexes.push_back({{ 0, 11,  5 }});
	tri_indexes.push_back({{ 0,  5,  1 }});
	tri_indexes.push_back({{ 0,  1,  7 }});
	tri_indexes.push_back({{ 0,  7, 10 }});
	tri_indexes.push_back({{ 0, 10, 11 }});

	// 5 adjacent tri_indexes.
	tri_indexes.push_back({{  1,  5, 9 }});
	tri_indexes.push_back({{  5, 11, 4 }});
	tri_indexes.push_back({{ 11, 10, 2 }});
	tri_indexes.push_back({{ 10,  7, 6 }});
	tri_indexes.push_back({{  7,  1, 8 }});

	// 5 tri_indexes around point 3.
	tri_indexes.push_back({{ 3, 9, 4 }});
	tri_indexes.push_back({{ 3, 4, 2 }});
	tri_indexes.push_back({{ 3, 2, 6 }});
	tri_indexes.push_back({{ 3, 6, 8 }});
	tri_indexes.push_back({{ 3, 8, 9 }});

	// 5 adjacent tri_indexes.
	tri_indexes.push_back({{ 4,  9,  5 }});
	tri_indexes.push_back({{ 2,  4, 11 }});
	tri_indexes.push_back({{ 6,  2, 10 }});
	tri_indexes.push_back({{ 8,  6,  7 }});
	tri_indexes.push_back({{ 9,  8,  1 }});

	// Refine triangles by recursing onto the tri_indexes and splitting
	// each triangle face to 4 individual triangles.
	for (int i = 0; i<recursion; i++) {
		std::vector<PSI::tri_indexes> split_tri_indexes;
		for (auto tri : tri_indexes) {
			// Split one triangle into 4.
			//
			// Calculate three middle points, for each side
			// noted a, b, c.
			//
			//       0
			//       . 
			//      / \
			//   a .   . c
			//    /     \
			//   .___.___.
			//  1    b    2
			//
			//
			// We need to store these triangles for normal processing .. 
			// We will be pushing 3 normals, as we are pushing 3 positions.
		
			// Calculate the middle points for side a.
			glm::vec3 p1_1 = geom->positions.at(tri[0]);
			glm::vec3 p1_2 = geom->positions.at(tri[1]);
			glm::vec3 mid1 = (p1_1 + p1_2) / 2.0f;
			geom->positions.push_back(glm::normalize(mid1));
			GLint a = vertex_count++;

			// Calculate the middle points for side b.
			glm::vec3 p2_1 = geom->positions.at(tri[1]);
			glm::vec3 p2_2 = geom->positions.at(tri[2]);
			glm::vec3 mid2 = (p2_1 + p2_2) / 2.0f;
			geom->positions.push_back(glm::normalize(mid2));
			GLint b = vertex_count++;

			// Calculate the middle points for side c.
			glm::vec3 p3_1 = geom->positions.at(tri[2]);
			glm::vec3 p3_2 = geom->positions.at(tri[0]);
			glm::vec3 mid3 = (p3_1 + p3_2) / 2.0f;
			geom->positions.push_back(glm::normalize(mid3));
			GLint c = vertex_count++;

			// Create the new triangle, with 4 faces
			// Using the previously calculated indexes.

			// Start pushing from the top triangle.
			//
			//       0
			//       . 
			//      /.\
			//   a ..... c
			//    /     \
			//   .___.___.
			//  1    b    2
			PSI::tri_indexes t0ac = {{ tri[0], a, c }};
			split_tri_indexes.push_back(t0ac);

			//       0
			//       . 
			//      / \
			//   a /   \ c
			//    /.    \
			//   .:::.___.
			//  1    b    2
			PSI::tri_indexes t1ab = {{ tri[1], b, a }};
			split_tri_indexes.push_back(t1ab);

			//       0
			//       . 
			//      / \
			//   a /   \ c
			//    /    :\
			//   .___.'::.
			//  1    b    2
			PSI::tri_indexes t2cb = {{ tri[2], c, b }};
			split_tri_indexes.push_back(t2cb);

			// The base formation, in the middle.
			// We will be calculating normals based on this and the side triangles.
			//       0
			//       . 
			//      / \
			//   a _____ c
			//    /\.o./\
			//   .__\:/__.
			//  1    b    2
			PSI::tri_indexes t3abc = {{ a, b, c }};
			split_tri_indexes.push_back(t3abc);
		}

		tri_indexes = split_tri_indexes;
	}

	// Add indexes..
	for (auto tri : tri_indexes) {
		geom->indexes.push_back((GLuint)tri[0]);
		geom->indexes.push_back((GLuint)tri[1]);
		geom->indexes.push_back((GLuint)tri[2]);
	}

	// We can just use the positions as the normals, 
	// as vertex point is a direct vector away from the center point.
	geom->normals = geom->positions;

	return geom;
}
} // namespace Icosahedron
} // namespace PSIGeometry
