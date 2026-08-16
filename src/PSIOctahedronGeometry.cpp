#include "PSIOctahedronGeometry.h"

namespace PSIGeometry {
	namespace Octahedron {

		// The texture coordinates every face gets, as an equilateral triangle
		// inscribed in the unit square and centred on (0.5, 0.5).
		//
		// Centred deliberately. The plasma these faces sample has a radial term
		// that is centred on the texture, so a triangle mapped into a corner of
		// the square would take an off-centre slice of it and the eight faces
		// would not read as one effect. Corners at 90, 210 and 330 degrees, half
		// a unit out.
		static const glm::vec2 face_texcoords[3] = {
			glm::vec2(0.5f,      1.0f),
			glm::vec2(0.066987f, 0.25f),
			glm::vec2(0.933013f, 0.25f),
		};

		GLint octant_index(GLint sx, GLint sy, GLint sz) {
			return (sx > 0 ? 1 : 0) | (sy > 0 ? 2 : 0) | (sz > 0 ? 4 : 0);
		}

		GeometryDataSharedPtr octahedron() {
			GeometryDataSharedPtr geom = PSIGeometryData::create();

			geom->positions.reserve(24);
			geom->normals.reserve(24);
			geom->texcoords.reserve(24);
			geom->indexes.reserve(24);

			// Emitted in octant-index order, so face n of the buffer is octant n.
			// Nothing reads it that way -- the shader picks its texture from the
			// normal -- but it makes the buffer legible next to octant_index().
			for (GLint octant = 0; octant < 8; octant++) {
				const GLint sx = (octant & 1) ? 1 : -1;
				const GLint sy = (octant & 2) ? 1 : -1;
				const GLint sz = (octant & 4) ? 1 : -1;

				const glm::vec3 vx = glm::vec3(0.5f * sx, 0.0f, 0.0f);
				const glm::vec3 vy = glm::vec3(0.0f, 0.5f * sy, 0.0f);
				const glm::vec3 vz = glm::vec3(0.0f, 0.0f, 0.5f * sz);

				// Winding. Taking the vertexes in X, Y, Z order is counter
				// clockwise seen from outside only when the signs multiply to
				// +1; flipping any one sign mirrors the triangle. Swapping the
				// last two puts it back, which is cheaper to read than eight
				// hand-written orderings and impossible to get subtly wrong.
				//
				// Front faces must be counter clockwise: the scene pass culls
				// back faces, and an inside-out diamond would show its far side
				// and nothing else.
				const bool flip = (sx * sy * sz) < 0;

				geom->positions.push_back(vx);
				geom->positions.push_back(flip ? vz : vy);
				geom->positions.push_back(flip ? vy : vz);

				// Flat shaded, so all three vertexes carry the face's normal.
				// For this shape that is just the octant's own direction.
				const glm::vec3 normal = glm::normalize(glm::vec3(sx, sy, sz));
				geom->normals.push_back(normal);
				geom->normals.push_back(normal);
				geom->normals.push_back(normal);

				geom->texcoords.push_back(face_texcoords[0]);
				geom->texcoords.push_back(face_texcoords[1]);
				geom->texcoords.push_back(face_texcoords[2]);

				const GLuint base = static_cast<GLuint>(octant * 3);
				geom->indexes.push_back(base + 0);
				geom->indexes.push_back(base + 1);
				geom->indexes.push_back(base + 2);
			}

			return geom;
		}

	} // namespace Octahedron
} // namespace PSIGeometry
