// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// A class containing data for a geometric object that is uploaded to the GPU.

#pragma once

#include "PSIGlobals.h"
#include "PSIGLMesh.h"

class PSIGeometryData {
	private:

	public:
		PSIGeometryData() = default;
		~PSIGeometryData() = default;

		// Only one of these should be used
		// either typed or buffer
		// typed is for data generated at runtime
		// buffer is for pregenerated OpenGL data from glTF buffers
	
		// Our geometry data in typed data
		std::vector<glm::vec3> positions;
		std::vector<glm::vec4> colors;
		std::vector<glm::vec2> texcoords;
		std::vector<glm::vec3> normals;
		std::vector<GLuint>    indexes;

		// Raw buffers
		std::vector<PSIGLMesh::gl_buffer_info> buffers;
		std::vector<PSIGLMesh::gl_vertex_attribute> attributes;

		// Methods
		void print_data();

}; // PSIGeometryData
