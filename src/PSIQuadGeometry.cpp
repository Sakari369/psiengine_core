#include "PSIQuadGeometry.h"

namespace PSIGeometry {
namespace Quad {

// Generate a quad.
std::array<glm::vec3, 4> quad(glm::vec2 origin, glm::vec2 radius) {
	std::array<glm::vec3, 4> quad;

	//  x0,y0  .____. x1, y1
	//         |    |
	//         |    |
	//         |    |
	//  x3,y3  '----' x2, y2
	//
	GLfloat x0 = origin.x - radius.x;
	GLfloat y0 = origin.y - radius.y;

	GLfloat x1 = origin.x + radius.x;
	GLfloat y1 = origin.y - radius.y;

	GLfloat x2 = origin.x + radius.x;
	GLfloat y2 = origin.y + radius.y;

	GLfloat x3 = origin.x - radius.x;
	GLfloat y3 = origin.y + radius.y;

	// We need to get the two triangles from this quad .
	//
	//    t0.0 .___. t0.1
	//  t1.0   :\  |
	//         : \ |
	//         :  \|
	//  t1.2   ....\ t0.2
	//
	//            t1.1

	// First triangle, t0, {0, 1, 2}, clockwise winding.
	//
	//  p[0] :--. p[1]              p[0] = {-1.0f,  -1.0f}
	//        \ |                   p[1] = { 0.5f,  0.0f}
	//         \|                   p[2] = { 0.5f,  0.5f}
	//          '
	//          p[2]
	//
	quad[0] = glm::vec3(x0, y0, 0.0f);
	quad[1] = glm::vec3(x1, y1, 0.0f);
	quad[2] = glm::vec3(x2, y2, 0.0f);

	// Second triangle, t1, {0, 2, 3}, clockwise winding.
	//
	// p[0] :\                      p[0] = {-1.0f, -1.0f}
	//      . \                     p[2] = { 1.0f,  1.0f}
	//      .  \                    p[3] = {-1.0f,  1.0f}
	// p[3] .___' p[2]
	// 
	quad[3] = glm::vec3(x3, y3, 0.0f);

	return quad;
}

std::array<GLuint, 6> calc_quad_indexes(GLint quad_index_offset) {
	// Copy the triangle indexes to indexed indexes.
	std::array<GLuint, 6> indexes = {{0,1,2, 0,2,3}};

	// Add offset to each vertex index, so that we can generate indexes
	// for a quad positioned at this index inside an array of indexes.
	GLint vertex_offset = 4 * quad_index_offset;

	indexes[0] += vertex_offset;
	indexes[1] += vertex_offset;
	indexes[2] += vertex_offset;
	indexes[3] += vertex_offset;
	indexes[4] += vertex_offset;
	indexes[5] += vertex_offset;

	return indexes;
}

} // namespace Quad
} // namespace PSIGeometry
