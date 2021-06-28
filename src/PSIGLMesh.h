// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Defines OpenGL mesh data that is uploaded to the GPU.

#pragma once
#include <iostream>
#include <math.h>

#include "PSIGlobals.h"
#include "PSIGLUtils.h"

class PSIGLMesh;
typedef shared_ptr<PSIGLMesh> GLMeshSharedPtr;

class PSIGLMesh {
	public:
		// Vertex attrib buffer names.
		enum BufferName {
			POSITION = 0,
			COLOR    = 1,
			TEXCOORD = 2,
			NORMAL   = 3,
			TANGENT  = 4,
			SEGMENT  = 5,
			ANGLE    = 6,
			INDEX    = 7,
			BufferName_MAX = INDEX
		};

		// General definitions.
		enum GLMeshDefs {
			INVALID_ATTRIB_LOCATION = -1
		};

		// OpenGL buffer information.
		struct gl_buffer_info {
			GLuint name_id;
			GLenum target;
			GLsizeiptr size;
			GLenum usage;
			const GLvoid *data;
		};

		// OpenGL vertex attribute data.
		struct gl_vertex_attribute {
			GLuint buffer_name_id;
			const GLchar *name;
			GLint size;
			GLenum type;
			GLboolean normalized;
			GLsizei stride;
			const GLvoid *pointer;
		};

		PSIGLMesh(GLuint vao = 0, GLsizei draw_count = 0, GLuint draw_mode = GL_TRIANGLES);
		~PSIGLMesh();

		static GLMeshSharedPtr create() {
			return make_shared<PSIGLMesh>();
		}

		bool init();

		// Draw unindexed.
		void draw();
		// Draw whole mesh indexed.
		void draw_indexed();
		// Draw indexed, beginning from index with count.
		void draw_indexed(GLuint offset, GLuint count);

		// Generate our vertex attribute object.
		void gen_vao();
		// Generate our vertex attribute buffers.
		void gen_buffers();

		// Bind one of the vertex attribute buffers.
		void bind_buffer(GLenum target, GLuint buffer_name_id);
		// Bind vao for this mesh.
		void bind_vao();

		// Enable vertex attrib with a location.
		void enable_vertex_attrib(GLuint location, GLint size, GLsizei stride,
		                          const GLvoid *pointer, GLenum type);
		// Enable vertex attrib with a name.
		GLuint enable_vertex_attrib(GLuint program, const GLchar *name, GLint size, GLsizei stride,
		                            const GLvoid *pointer, GLenum type);

		// Update buffer subdata.
		void buffer_sub_data(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data) {
			glBufferSubData(target, offset, size, data);
		}

		// Update buffer data.
		void buffer_data(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage) {
			glBufferData(target, size, data, usage);
		}

		// Get current buffer id.
		GLuint get_buffer_id(GLuint buffer_name_id) {
			return _buffer_name_ids[buffer_name_id];
		}

		void set_draw_count(GLuint draw_count) {
			_draw_count = draw_count;
		}
		GLsizei get_draw_count() {
			return _draw_count;
		}

		void set_draw_mode(GLuint draw_mode) {
			_draw_mode = draw_mode;
		}
		GLuint get_draw_mode() {
			return _draw_mode;
		}

		void set_index_type(GLenum type) {
			_index_type = type;
		}
		GLenum get_index_type() {
			return _index_type;
		}
	private:
		// Reference to the vertex array object for this mesh.
		GLuint _vao;
		// How many vertexes are we drawing ?
		GLsizei _draw_count = 0;
		// What of the gl draw modes are we using ?
		// GL_LINES, GL_TRIANGLES, GL_POINTS ?
		GLuint _draw_mode = GL_TRIANGLES;
		// The index component type.
		GLenum _index_type = GL_UNSIGNED_INT;
		// Reference to vertex buffer object.
		GLuint _buffer_name_ids[BufferName::BufferName_MAX + 1];
};
