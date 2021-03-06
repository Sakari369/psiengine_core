// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// A class containing data for a geometric object that is uploaded to the GPU.

#pragma once

#include "PSIGlobals.h"
#include "PSIGLMesh.h"

class PSIGeometryData;
typedef shared_ptr<PSIGeometryData> GeometryDataSharedPtr;

class PSIGeometryData {
	private:

	public:
		PSIGeometryData() = default;
		~PSIGeometryData() = default;

		static GeometryDataSharedPtr create() {
			return make_shared<PSIGeometryData>();
		}

		// Only one of these should be used.
		// either typed data or raw buffers.
		// Typed is for data generated at runtime.
		// Raw buffers are for pregenerated OpenGL data from glTF buffers.
	
		// Our geometry data in typed data.
		std::vector<glm::vec3> positions;
		std::vector<glm::vec4> colors;
		std::vector<glm::vec2> texcoords;
		std::vector<glm::vec3> normals;
		std::vector<GLuint>    indexes;

		// Raw buffers.
		std::vector<PSIGLMesh::gl_buffer_info> buffers;
		std::vector<PSIGLMesh::gl_vertex_attribute> attributes;

		// Methods
		void print_data();

}; // PSIGeometryData
