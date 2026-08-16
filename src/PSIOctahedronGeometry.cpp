#include "PSIOctahedronGeometry.h"

namespace PSIGeometry {
	namespace Octahedron {

		// Texture coordinates, assigned per VERTEX rather than per face.
		//
		// This is what makes a textured octahedron read as one surface instead
		// of eight tiles. Two faces sharing an edge share two vertexes, so if
		// the coordinates belong to the vertexes then both faces interpolate
		// the same values along that edge and the texture crosses it without a
		// seam -- by construction, for every edge, with nothing to line up by
		// hand. Giving every face its own triangle of the square (which this
		// did first) puts the same patch on all eight, identically oriented, so
		// the shape reads as tiled no matter how good the texture is.
		//
		// The layout: the four equatorial vertexes go to the square's corners
		// and both poles to its centre, so each face covers one quarter of the
		// square and the eight of them cover it twice.
		//
		//        -X (0,1) +-----------+ +Y (1,1)
		//                 | \       / |
		//                 |   \   /   |
		//                 |     X     |     <- both Z poles, at (0.5, 0.5)
		//                 |   /   \   |
		//                 | /       \ |
		//        -Y (0,0) +-----------+ +X (1,0)
		//
		// The diagonal pairing is forced, not chosen. Every face takes one
		// vertex from each axis pair, so all four X-Y combinations occur as
		// edges -- and they can only all be square edges if +X/-X are opposite
		// corners and +Y/-Y are the other two.
		//
		// Both poles landing on the same coordinate is deliberate too: the two
		// faces either side of an equatorial edge then sample the same triangle,
		// which is the mirroring that makes them continue into each other.
		static inline glm::vec2 equator_uv(GLint sign, bool is_x) {
			if (is_x) {
				return sign > 0 ? glm::vec2(1.0f, 0.0f) : glm::vec2(0.0f, 1.0f);
			}
			return sign > 0 ? glm::vec2(1.0f, 1.0f) : glm::vec2(0.0f, 0.0f);
		}

		static const glm::vec2 pole_uv = glm::vec2(0.5f, 0.5f);

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

				// Position and coordinate go together, because the coordinate
				// belongs to the vertex -- emitting them apart is how the two
				// fall out of step when the winding swaps the last pair.
				const glm::vec2 uvx = equator_uv(sx, true);
				const glm::vec2 uvy = equator_uv(sy, false);

				geom->positions.push_back(vx);
				geom->texcoords.push_back(uvx);

				geom->positions.push_back(flip ? vz : vy);
				geom->texcoords.push_back(flip ? pole_uv : uvy);

				geom->positions.push_back(flip ? vy : vz);
				geom->texcoords.push_back(flip ? uvy : pole_uv);

				// Flat shaded, so all three vertexes carry the face's normal.
				// For this shape that is just the octant's own direction.
				const glm::vec3 normal = glm::normalize(glm::vec3(sx, sy, sz));
				geom->normals.push_back(normal);
				geom->normals.push_back(normal);
				geom->normals.push_back(normal);

				const GLuint base = static_cast<GLuint>(octant * 3);
				geom->indexes.push_back(base + 0);
				geom->indexes.push_back(base + 1);
				geom->indexes.push_back(base + 2);
			}

			return geom;
		}

	} // namespace Octahedron
} // namespace PSIGeometry
