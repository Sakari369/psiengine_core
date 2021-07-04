#include "PSICubeGeometry.h"

namespace PSIGeometry {
	namespace Cube {
		// All the vertex points that define a cube
		// in 3d space.
		//
		// These are defined counterclockwise.
		// As it's the default direction for front facing triangles.
		//
		//     3 .______________. 0
		//      /|             /|
		//     / |            / | 
		//  6 .--------------. 5|
		//    |  |           |  |
		//    |  |           |  |
		//    |  |     o     |  |
		//    |  |           |  |
		//    |2 .___________|__. 1
		//    | /            | /
		//  7 ./_____________./ 4
		//
		//
		const std::vector<glm::vec3> verts {
			// Back face vertexes.
			glm::vec3( 0.5f, -0.5f, -0.5f), // 0
			glm::vec3( 0.5f,  0.5f, -0.5f), // 1
			glm::vec3(-0.5f,  0.5f, -0.5f), // 2
			glm::vec3(-0.5f, -0.5f, -0.5f), // 3

			// Front face vertexes.
			glm::vec3( 0.5f, -0.5f,  0.5f), // 4
			glm::vec3( 0.5f,  0.5f,  0.5f), // 5
			glm::vec3(-0.5f,  0.5f,  0.5f), // 6
			glm::vec3(-0.5f, -0.5f,  0.5f), // 7
		};

		// Cube faces, vertex points for each face quad
		// Frontfacing are counterclockwise wound.
		// Backfacing are clockwise wound.
		//
		// Counter clockwise winding
		// 0    3
		// o----o
		// |    |
		// |    |
		// o----o
		// 1    2
		//
		// Clockwise winding
		// 0    1
		// o----o
		// |    |
		// |    |
		// o----o
		// 3    2
	
		// Helper to make it more clear which vertexes to take into the positions
		const std::vector<PSI::quad_buf> faces = {
			{{ verts[6], verts[7], verts[4], verts[5] }}, // front, ccw
			{{ verts[2], verts[1], verts[0], verts[3] }}, // back, cw
			{{ verts[5], verts[4], verts[0], verts[1] }}, // right, ccw
			{{ verts[6], verts[2], verts[3], verts[7] }}, // left, cw
			{{ verts[3], verts[0], verts[4], verts[7] }}, // bottom, cw
			{{ verts[2], verts[6], verts[5], verts[1] }}  // top, ccw
		};

		// Actual positions that we upload
		const std::vector<glm::vec3> positions = {
			faces[0][0], faces[0][1], faces[0][2], faces[0][3], // front
			faces[1][0], faces[1][1], faces[1][2], faces[1][3], // back
			faces[2][0], faces[2][1], faces[2][2], faces[2][3], // right
			faces[3][0], faces[3][1], faces[3][2], faces[3][3], // left
			faces[4][0], faces[4][1], faces[4][2], faces[4][3], // bottom
			faces[5][0], faces[5][1], faces[5][2], faces[5][3], // t
		};

		// indexes for the 4 vertexes per face
		// clockwise 	    = 0, 1, 2, 0, 2, 3
		const std::vector<GLuint> indexes = {
			0*4+0, 0*4+1, 0*4+2, // front
			0*4+0, 0*4+2, 0*4+3, 
			
			1*4+0, 1*4+1, 1*4+2, // back
			1*4+0, 1*4+2, 1*4+3, 

			2*4+0, 2*4+1, 2*4+2, // right
			2*4+0, 2*4+2, 2*4+3, 

			3*4+0, 3*4+1, 3*4+2, // left
			3*4+0, 3*4+2, 3*4+3, 

			4*4+0, 4*4+1, 4*4+2, // bottom
			4*4+0, 4*4+2, 4*4+3, 

			5*4+0, 5*4+1, 5*4+2, // top
			5*4+0, 5*4+2, 5*4+3,
		};

		// counterclock wise = 2, 1, 0, 3, 2, 0
		// for cubemaps or inverted cubes that you are inside
		const std::vector<GLuint> indexes_ccw = {
			0*4+2, 0*4+1, 0*4+0, // front
			0*4+3, 0*4+2, 0*4+0, 
			
			1*4+2, 1*4+1, 1*4+0, // back
			1*4+3, 1*4+2, 1*4+0, 

			2*4+2, 2*4+1, 2*4+0, // right
			2*4+3, 2*4+2, 2*4+0, 

			3*4+2, 3*4+1, 3*4+0, // left
			3*4+3, 3*4+2, 3*4+0, 

			4*4+2, 4*4+1, 4*4+0, // bottom
			4*4+3, 4*4+2, 4*4+0, 

			5*4+2, 5*4+1, 5*4+0, // top
			5*4+3, 5*4+2, 5*4+0,
		};

		// normals for all the 6 * 4 vertexes
		const std::vector<glm::vec3> normals = {
			// front
			glm::vec3( 0.0f,  0.0f, +1.0f),
			glm::vec3( 0.0f,  0.0f, +1.0f),
			glm::vec3( 0.0f,  0.0f, +1.0f),
			glm::vec3( 0.0f,  0.0f, +1.0f),
			// back
			glm::vec3( 0.0f,  0.0f, -1.0f),
			glm::vec3( 0.0f,  0.0f, -1.0f),
			glm::vec3( 0.0f,  0.0f, -1.0f),
			glm::vec3( 0.0f,  0.0f, -1.0f),
			// right
			glm::vec3(+1.0f,  0.0f,  0.0f),
			glm::vec3(+1.0f,  0.0f,  0.0f),
			glm::vec3(+1.0f,  0.0f,  0.0f),
			glm::vec3(+1.0f,  0.0f,  0.0f),
			// left
			glm::vec3(-1.0f,  0.0f,  0.0f),
			glm::vec3(-1.0f,  0.0f,  0.0f),
			glm::vec3(-1.0f,  0.0f,  0.0f),
			glm::vec3(-1.0f,  0.0f,  0.0f),
			// bottom
			glm::vec3( 0.0f, -1.0f,  0.0f),
			glm::vec3( 0.0f, -1.0f,  0.0f),
			glm::vec3( 0.0f, -1.0f,  0.0f),
			glm::vec3( 0.0f, -1.0f,  0.0f),
			// top
			glm::vec3( 0.0f, +1.0f,  0.0f),
			glm::vec3( 0.0f, +1.0f,  0.0f),
			glm::vec3( 0.0f, +1.0f,  0.0f),
			glm::vec3( 0.0f, +1.0f,  0.0f),
		};

		// Texture co-ordinates
		//   1
		//   .______,
		//   |     /|
		//   |    / |
		//   |   /  |
		// v |  /   |
		//   | /    |
		//   |/     |
		//   '------'  
		//   0   u   1
		//

		// These are for mapping a single texture with the same alignment on all sides
		//
		// Because we are drawing our cube face triangles
		// In counter clockwise order
		//
		// Like this:
		//
		//  2.______,1
		//   |\     |
		//   | \    |
		//   |  \   |
		//   |   \  |
		//   |    \ |
		//   |     \|
		//   '------'  
		//  3        0
		//
		// We need to match the texture co-ordinate U,V co-ordinates to the indexed
		// vertexes {0, 1, 2, 3}
		//
		// So we just flip the tex-coordinates like this:
		//
		// Texture co-ordinates
		//
		//   0  v   1
		//   .______,
		//   |\     |
		//   | \    |
		//   |  \   |
		//   |   \  | u
		//   |    \ |
		//   |     \|
		//   '------'  
		//           0
		//
		//   And we get:
		//
		//   vertex 0, u = 0, v = 0
		//   vertex 1, u = 0, v = 1
		//   vertex 2, u = 1, v = 1
		//   vertex 3, u = 1, v = 0
		//
		const std::vector<glm::vec2> texcoords = {
			// front
			glm::vec2(0.0f, 0.0f),
			glm::vec2(0.0f, 1.0f),
			glm::vec2(1.0f, 1.0f),
			glm::vec2(1.0f, 0.0f),

			// back
			glm::vec2(1.0f, 0.0f),
			glm::vec2(0.0f, 0.0f),
			glm::vec2(0.0f, 1.0f),
			glm::vec2(1.0f, 1.0f),

			// right
			glm::vec2(0.0f, 0.0f),
			glm::vec2(0.0f, 1.0f),
			glm::vec2(1.0f, 1.0f),
			glm::vec2(1.0f, 0.0f),

			// left
			glm::vec2(1.0f, 0.0f),
			glm::vec2(0.0f, 0.0f),
			glm::vec2(0.0f, 1.0f),
			glm::vec2(1.0f, 1.0f),

			// bottom
			glm::vec2(0.0f, 1.0f),
			glm::vec2(1.0f, 1.0f),
			glm::vec2(1.0f, 0.0f),
			glm::vec2(0.0f, 0.0f),

			// top
			glm::vec2(0.0f, 0.0f),
			glm::vec2(0.0f, 1.0f),
			glm::vec2(1.0f, 1.0f),
			glm::vec2(1.0f, 0.0f),
		};

	} // namspace Cube
} // namespace PSIGeometry
