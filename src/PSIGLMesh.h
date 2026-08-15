// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Mesh data uploaded to the GPU. Metal backend.
//
// The public API is deliberately unchanged: PSIGLMesh is bound to Lua
// (LuaAPI.cpp:557) and PSIRenderObj::init_buffers drives it with OpenGL's
// bind-then-upload idiom, which is reproduced here rather than rewritten.
//
// The layout maps onto Metal unusually well. The GL version kept one VBO per
// attribute instead of interleaving, so each attribute buffer simply becomes an
// MTLBuffer bound at the index matching its BufferName -- which is also the
// [[attribute(n)]] index in the shaders.
//
// Buffer target/usage/type fields are still the GL enums the geometry
// generators produce (PSIGeometry::add_buffer_defaults). They are interpreted
// here rather than removed, so the 9 procedural geometry generators need no
// changes.

#pragma once
#include <iostream>
#include <math.h>

#include "PSIGlobals.h"
#include "PSIGLUtils.h"

class PSIGLMesh;
typedef shared_ptr<PSIGLMesh> GLMeshSharedPtr;

class PSIGLMesh {
	public:
		// Vertex attrib buffer names. Also the Metal buffer index and the
		// [[attribute(n)]] index -- see PSIMetal::BufferIndex.
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

		// Buffer information, as produced by PSIGeometry::add_buffer_defaults().
		// Field names and GL enum values are kept for source compatibility.
		struct gl_buffer_info {
			GLuint name_id;
			GLenum target;
			GLsizeiptr size;
			GLenum usage;
			const GLvoid *data;
		};

		// Vertex attribute data.
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

		// Draw `instances` copies. Used by the instanced replacements for the
		// geometry shaders, which amplify one point into a fixed vertex count.
		void draw_instanced(GLuint vertex_count, GLuint instances);

		// No-ops: Metal has no vertex array objects, and buffers are allocated
		// on upload rather than reserved up front. Kept so the existing
		// create_gl_mesh() sequence still reads the same.
		void gen_vao();
		void gen_buffers();
		void bind_vao();

		// Select which buffer subsequent buffer_data()/buffer_sub_data() calls
		// and the next enable_vertex_attrib() apply to.
		void bind_buffer(GLenum target, GLuint buffer_name_id);

		// Enable vertex attrib with a location.
		void enable_vertex_attrib(GLuint location, GLint size, GLsizei stride,
		                          const GLvoid *pointer, GLenum type);
		// Enable vertex attrib with a name.
		GLuint enable_vertex_attrib(GLuint program, const GLchar *name, GLint size, GLsizei stride,
		                            const GLvoid *pointer, GLenum type);

		// Update buffer contents in place.
		void buffer_sub_data(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data);

		// Allocate or replace a buffer's contents.
		void buffer_data(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage);

		// Get current buffer id. No longer a GL name, but stable and non-zero
		// for buffers that exist, which is all the callers check.
		GLuint get_buffer_id(GLuint buffer_name_id);

		MTL::Buffer *get_buffer(GLuint buffer_name_id) {
			if (buffer_name_id > BufferName_MAX) {
				return nullptr;
			}
			return _buffers[buffer_name_id];
		}

		void set_draw_count(GLuint draw_count) {
			_draw_count = draw_count;
		}
		GLsizei get_draw_count() {
			return _draw_count;
		}

		// Draw mode is a raw GL primitive enum, including from Lua --
		// normal_vis.lua calls set_draw_mode(0) meaning GL_POINTS. Mapped to
		// MTLPrimitiveType at draw time.
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
		// One buffer per attribute, plus the index buffer at BufferName::INDEX.
		MTL::Buffer *_buffers[BufferName::BufferName_MAX + 1] = {};

		// Which attributes actually have data bound. Attributes the shader
		// declares but the mesh never filled would otherwise be left unbound.
		bool _attrib_enabled[BufferName::BufferName_MAX + 1] = {};

		// Emulates OpenGL's bind-then-upload: which buffer the next
		// buffer_data()/enable_vertex_attrib() call refers to, per target.
		GLuint _bound_array_name = BufferName::POSITION;
		GLuint _bound_index_name = BufferName::INDEX;

		// How many vertexes are we drawing ?
		GLsizei _draw_count = 0;
		// Primitive type, as a GL enum.
		GLuint _draw_mode = GL_TRIANGLES;
		// The index component type, as a GL enum.
		GLenum _index_type = GL_UNSIGNED_INT;

		// Bind every populated attribute buffer on the active encoder.
		void bind_vertex_buffers(MTL::RenderCommandEncoder *encoder);
		// Upload the bound shader's staged uniform block.
		void flush_current_uniforms();
		// Resolve the target enum to the buffer name currently bound to it.
		GLuint bound_name_for(GLenum target) const;
};
