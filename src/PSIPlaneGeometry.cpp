#include "PSIPlaneGeometry.h"
#include "PSIQuadGeometry.h"

namespace PSIGeometry {
namespace Plane {

GeometryDataSharedPtr uniform_plane(GLint rows, GLboolean repeat_texture) {
	GeometryDataSharedPtr data = PSIGeometryData::create();
	if (data == nullptr) {
		return nullptr;
	}

	// How many grid faces (quads) do we have ?
	GLint quad_count = rows * rows;

	// 4 indexed vertexes in two triangles that make the quad.
	GLint vertex_count  = 4 * quad_count;

	// 6 indexes per face ({0, 1, 2, 0, 2, 3}).
	GLint indexes_count = 6 * vertex_count;
	std::array<GLuint, 6> vertex_offsets;

	// Reserve our data.
	data->positions.reserve(vertex_count);
	data->normals.reserve(vertex_count);
	data->texcoords.reserve(vertex_count);
	data->indexes.reserve(indexes_count);

	// Quad dimensions.
	// From -0.5 .. 0.5 = 1.0.
	GLfloat w = 1.0f / rows;
	GLfloat h = 1.0f / rows;

	// Quad vertex point offsets.
	GLfloat xoff = w / 2.0f;
	GLfloat yoff = h / 2.0f;

	glm::vec2 cell_radius = {xoff, yoff};

	plog_s("rows = %d w = %f h = %f", rows, w, h);

	// Create a grid.
	// That goes from -1.0 .. 1.0 in the x axis
	//            and -1.0 .. 1.0 in the y axis
	// So that these axises are divided into rows equally sized quads.
	// Generate two triangles for each row item.
	glm::vec2 origin = { 
		-0.5f + xoff,
		-0.5f + yoff,
	};

	GLint vertex_offset;
	GLint quad_index = 0;

	// Texcoord u and v bases for each quad.
	GLfloat ub = 0.0f;
	GLfloat vb = 1.0f;

	for (int cy=0; cy < rows; cy++) {
		origin.x = -0.5f + xoff;
		ub = 0.0f;

		for (int cx=0; cx < rows; cx++) {
			// Create a quad.
			std::array<glm::vec3, 4> quad = PSIGeometry::Quad::quad(origin, cell_radius);

			// Positions.
			data->positions.push_back(quad[0]);
			data->positions.push_back(quad[1]);
			data->positions.push_back(quad[2]);
			data->positions.push_back(quad[3]);

			// Texture co-ordinates.
			//   uv0      uv1
			//
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
			//   uv3      uv2
			//

			/*
			plog_s("cx = %d cy = %d origin.x = %f origin.y = %f", 
				cx, cy, origin.x, origin.y);

			plog_s("\nq[0] = %s\nq[1] = %s\nq[2] = %s\nq[3] = %s", 
				GLM_CSTR(quad[0]), GLM_CSTR(quad[1]), GLM_CSTR(quad[2]), GLM_CSTR(quad[3]));
			plog_s("ub = %f vb = %f", ub, vb);
				*/

			glm::vec2 uv0, uv1, uv2, uv3;

			// If we want to repeat the same texture on each quad, just generate the same texture coordinates 
			// for each quad on the plane.
			if (repeat_texture == true) {
				uv0 = { 0.0f, 1.0f };
				uv1 = { 1.0f, 1.0f };
				uv2 = { 1.0f, 0.0f };
				uv3 = { 0.0f, 0.0f };
			// If we want to map the texture to the plane regardless of the number of quads on the plane
			// calculate the individual texture-coordinates for each quad.
			} else {
				uv0 = { ub, vb };
				uv1 = { ub + w, vb };
				uv2 = { ub + w, vb -h };
				uv3 = { ub, vb - h };
			}

			data->texcoords.push_back(uv0);
			data->texcoords.push_back(uv1);
			data->texcoords.push_back(uv2);
			data->texcoords.push_back(uv3);

			// Calculate the normals.
			PSI::triangle t0 = {{ quad[0], quad[1], quad[2] }};
			PSI::triangle t1 = {{ quad[0], quad[2], quad[3] }};
			glm::vec3 tri0_normal = PSIMath::calc_tri_normal(t0);
			glm::vec3 tri1_normal = PSIMath::calc_tri_normal(t1);

			// We need one vertex normal for each vertex point.
			data->normals.push_back(tri0_normal);
			data->normals.push_back(tri0_normal);
			data->normals.push_back(tri1_normal);
			data->normals.push_back(tri1_normal);

			/*
			   plog_s("a0 = %s b0 = %s c0 = %s", GLM_CSTR(a0), GLM_CSTR(b0), GLM_CSTR(c0));
			   plog_s("a1 = %s b1 = %s c1 = %s", GLM_CSTR(a1), GLM_CSTR(b1), GLM_CSTR(c1));
			   plog_s("tri0Normal = %s", glm::to_string(tri0Normal).c_str());
			   plog_s("tri1Normal = %s", glm::to_string(tri1Normal).c_str());
			 */

			// Calculate vertex indexes.
			vertex_offsets = PSIGeometry::Quad::calc_quad_indexes(quad_index);
			vertex_offset = 6 * quad_index;
			data->indexes.insert(data->indexes.begin() + vertex_offset, vertex_offsets.begin(), vertex_offsets.end());

			origin.x += w;
			ub += w;
			quad_index++;
		}

		vb -= h;
		origin.y += h;
	}

	return data;
};

std::vector<glm::vec3> line_grid(GLint rows) {
	std::vector<glm::vec3> vertexes;
	GLfloat step = 2.0f / rows;
	GLfloat x = -1.0f;
	GLfloat y = -1.0f;
	GLfloat z =  0.0f;

	// Horizontal lines.
	for (int i=0; i<rows; i++) {
		vertexes.push_back(glm::vec3(x, y, z));
		// Another edge of x.
		x *= -1.0f;
		vertexes.push_back(glm::vec3(x, y, z));

		// Walk the y path.
		y += step;
	}

	vertexes.push_back(glm::vec3(-1.0f, 1.0f, 0.0f));
	vertexes.push_back(glm::vec3(1.0f, 1.0f, 0.0f));

	x = -1.0f;
	z =  0.0f;
	y = -1.0f;
	// Vertical lines.
	for (int i=0; i<rows; i++) {
		vertexes.push_back(glm::vec3(x, y, z));
		// Another edge of y.
		y *= -1.0f;
		vertexes.push_back(glm::vec3(x, y, z));

		// Walk the x path.
		x += step;
	}
	vertexes.push_back(glm::vec3(1.0f, -1.0f, 0.0f));
	vertexes.push_back(glm::vec3(1.0f, 1.0f, 0.0f));

	return vertexes;
};
} // namespace Plane
} // namespace PSIGeometry
