#include "PSICubeGeometry.h"

namespace PSIGeometry {
	namespace Tetrahedron {

		// Tetrahedron aligned so that tip is pointing in the positive Z direction, towards the camera view.
		const std::vector<glm::vec3> verts = {
			glm::vec3( -sqrt(2.0/9.0f), sqrt(2.0/3.0f), -1.0/3.0f ), // 0, left
			glm::vec3( sqrt(8.0/9.0f), 0.0f, -1.0/3.0f ), // 1, right.
			glm::vec3( -sqrt(2.0/9.0f), -sqrt(2.0/3.0f), -1.0/3.0f ), // 1, down.
			glm::vec3( 0.0f, 0.0f, 1.0f ), // 3, tip.
		};

		// Faces.
		const std::vector<PSI::triangle> faces = {
			{{ verts[0], verts[3], verts[1] }}, // right top, ccw.
			{{ verts[3], verts[2], verts[1] }}, // right down, ccw.
			{{ verts[0], verts[2], verts[3] }}, // left, ccw.
			{{ verts[0], verts[1], verts[2] }}, // back, cw.
		};

		const std::vector<glm::vec3> positions = {
			faces[0].p[0], faces[0].p[1], faces[0].p[2], // front right.
			faces[1].p[0], faces[1].p[1], faces[1].p[2], // front left.
			faces[2].p[0], faces[2].p[1], faces[2].p[2], // back.
			faces[3].p[0], faces[3].p[1], faces[3].p[2], // bottom.
		};

		const std::vector<GLuint> indexes = {
			0, 1, 2, // front right, ccw.
			3, 4, 5, // front left, ccw.
			6, 7, 8, // back, cw.
			9, 10, 11, // bottom, cw.
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

			// left.
			glm::vec2(0.0f, 0.0f),
			glm::vec2(0.0f, 1.0f),
			glm::vec2(1.0f, 1.0f),

			// bottom.
			glm::vec2(0.0f, 0.0f),
			glm::vec2(0.0f, 1.0f),
			glm::vec2(1.0f, 1.0f),
		};
	} // namespace Tetrahedron

	namespace CubeTetrahedron {
		// All the vertex points that define a tethrahedron
		// in 3d space.
		//
		// These are defined counterclockwise.
		// As it's the default direction for front facing triangles.
		//
		// A tetrahedron can be calculated by taking three vertexes of a cube,
		// and connecting those to a fourth vertex point.
		//
		//
		//     0 .______________.  
		//      /|             /|
		//     / |            / | 
		//    .--------------. 3|
		//    |  |           |  |
		//    |  |           |  |
		//    |  |     o     |  |
		//    |  |           |  |
		//    |  .___________|__. 2
		//    | /            | /
		//  1 ./_____________./  
		//
		const std::vector<glm::vec3> verts = {
			// Base triangle.
			glm::vec3(-0.5f,  0.5f, -0.5f), // 0
			glm::vec3(-0.5f, -0.5f,  0.5f), // 1
			glm::vec3( 0.5f, -0.5f, -0.5f), // 2
			// Tip.
			glm::vec3( 0.5f,  0.5f,  0.5f), // 3
		};

		const std::vector<PSI::triangle> faces = {
			{{ verts[3], verts[1], verts[2] }}, // front right, ccw
			{{ verts[0], verts[1], verts[3] }}, // front left, ccw
			{{ verts[2], verts[0], verts[3] }}, // back, cw
			{{ verts[0], verts[2], verts[1] }}, // bottom, cw
		};

		const std::vector<glm::vec3> positions = {
			faces[0].p[0], faces[0].p[1], faces[0].p[2], // front right.
			faces[1].p[0], faces[1].p[1], faces[1].p[2], // front left.
			faces[2].p[0], faces[2].p[1], faces[2].p[2], // back.
			faces[3].p[0], faces[3].p[1], faces[3].p[2], // bottom.
		};

		const std::vector<glm::vec2> texcoords = {
			// front
			glm::vec2(0.0f, 0.0f),
			glm::vec2(0.0f, 1.0f),
			glm::vec2(1.0f, 1.0f),
			glm::vec2(1.0f, 0.0f),
		};
	} // namespace Tetrahedron

} // namespace PSIGeometry
