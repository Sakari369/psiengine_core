// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Shader interface. Metal backend.
//
// The public API is unchanged from the OpenGL version on purpose: PSIGLShader is
// bound to Lua (LuaAPI.cpp:173) and every script's load_shaders() drives it
// directly with add_from_file() / compile() / add_uniforms(). What changed is
// everything underneath.
//
// How uniforms work now
// ---------------------
// OpenGL let you set uniforms one at a time on a bound program; Metal binds a
// whole buffer. The bridge is reflection, which is what the GL path effectively
// did too -- add_uniforms() used to walk GL_ACTIVE_UNIFORMS and cache
// name -> location. It now walks the pipeline's uniform struct via
// MTLRenderPipelineReflection and caches name -> byte offset.
//
// set_uniform("u_name", value) writes into a CPU-side staging block instead of
// calling glUniform*, and bind_uniforms() uploads it once before the draw. The
// call sites, and therefore the Lua API, do not change.
//
// Shader source
// -------------
// Scripts still name GLSL files ("phong.vert", "phong.frag"). Those names are
// resolved to entry points in the precompiled psishaders.metallib by convention:
//
//     phong.vert -> vertex_phong
//     phong.frag -> fragment_phong
//
// A GEOMETRY stage is accepted and recorded but never compiled -- Metal has no
// geometry shaders. Those shaders are reimplemented as instanced vertex shaders,
// so requesting one selects the "_instanced" vertex variant instead of failing,
// which keeps poly scripts loading.

#pragma once

#include <iostream>
#include <math.h>
#include <unordered_map>
#include <unordered_set>

#include "PSIGlobals.h"
#include "PSIOpenGL.h"
#include "PSIFileUtils.h"
#include "PSIGLUtils.h"

class PSIGLShader;
typedef shared_ptr<PSIGLShader> ShaderSharedPtr;

class PSIGLShader {
	public:
		enum ShaderDefs {
			INVALID_SHADER = -1,
			INVALID_UNIFORM = -1,
			LINK_FAILED = 0,
			COMPILE_FAILED = 1,
		};

		enum ShaderType {
			VERTEX = 0,
			GEOMETRY,
			FRAGMENT,
		};

		// Fixed vertex attribute locations in our shaders.
		enum AttribLocation {
			INVALID  = -1,
			POSITION = 0,
			COLOR    = 1,
			TEXCOORD = 2,
			NORMAL   = 3,
			TANGENT  = 4,
			SEGMENT  = 5,
			ANGLE    = 6,
			attribLocation_MAX = ANGLE
		};

		PSIGLShader() = default;
		~PSIGLShader();

		static ShaderSharedPtr create() {
			return make_shared<PSIGLShader>();
		}

		// A uniform resolved by reflection: where it lives in the block and how
		// to convert the CPU-side glm type into Metal's layout.
		struct uniform_member {
			std::string name;
			// Byte offset into the uniform block.
			uint32_t offset = 0;
			// MTL::DataType, kept as a plain int so this header does not force
			// metal-cpp on every includer.
			uint32_t data_type = 0;
		};

		// Setting shader uniforms.
		template <typename Type>
		void set_uniform(const std::string &name, Type &&value) {
			GLuint location = get_uniform(name);
			if (location == (GLuint)INVALID_UNIFORM) {
				// The GL version indexed _uniforms with operator[], so a typo
				// silently inserted 0 and wrote to location 0, corrupting
				// whichever uniform happened to live there. Warn once instead.
				warn_missing_uniform(name);
				return;
			}
			set_uniform(location, std::forward<Type>(value));
		}

		// Setting static vertex attribute.
		template <typename Type>
		void vertex_attrib(GLuint index, Type &&value) {
			set_vertex_attrib(index, std::forward<Type>(value));
		}

		// Make this shader's pipeline state current on the active encoder.
		void use_program();

		// Return this uniform's index, or INVALID_UNIFORM if the shader has no
		// such uniform.
		GLuint get_uniform(const std::string &name) {
			auto it = _uniforms.find(name);
			if (it == _uniforms.end()) {
				return (GLuint)INVALID_UNIFORM;
			}
			return it->second;
		}

		// Setting different type uniform values, by resolved index.
		void set_uniform(GLuint location, const GLint &val);
		void set_uniform(GLuint location, const GLuint &val);
		void set_uniform(GLuint location, const GLfloat &val);
		void set_uniform(GLuint location, const glm::vec2 &vec);
		void set_uniform(GLuint location, const glm::vec3 &vec);
		void set_uniform(GLuint location, const glm::vec4 &vec);
		void set_uniform(GLuint location, const glm::mat3 &mat);
		void set_uniform(GLuint location, const glm::mat4 &mat);

		// Vertex attribute setting.
		//
		// glVertexAttrib* supplied a constant value for an attribute array that
		// was not enabled. Metal has no equivalent; the value is stashed and
		// applied by the mesh when the matching buffer is absent.
		void set_vertex_attrib(GLuint index, const glm::vec2 &vec);
		void set_vertex_attrib(GLuint index, const glm::vec3 &vec);
		void set_vertex_attrib(GLuint index, const glm::vec4 &vec);
		glm::vec4 get_static_vertex_attrib(GLuint index) const;
		bool has_static_vertex_attrib(GLuint index) const;

		// Add uniform variable for this shader.
		GLuint add_uniform(std::string name);
		// Reflect the pipeline's uniform block. Returns the uniform count.
		GLuint add_uniforms();
		// Transform feedback has no Metal equivalent; recorded for the Poly
		// compute-based reimplementation.
		void add_transform_feedback_varyings(std::vector<std::string> varyings, GLboolean interleaved);

		// Select the metallib entry point for this stage from a GLSL file name.
		bool add_from_file(ShaderType type, std::string shader_path);
		// Runtime shader source. Not supported with a precompiled metallib.
		bool add_from_string(ShaderType type, std::string shader_str);

		// Build the render pipeline state from the selected functions.
		GLuint compile();
		// Kept for API compatibility; pipeline creation happens in compile().
		GLuint create_program();

		// Get current shader program id.
		GLuint get_program() {
			return _program;
		}

		// Upload the staged uniform block to the active encoder. Called by the
		// draw path just before issuing the draw.
		void bind_uniforms();

		// Does this shader want the instanced geometry-shader replacement path?
		bool has_geometry_stage() const {
			return _has_geometry_stage;
		}

		// Name for display.
		void set_name(std::string name) {
			_name = name;
		}
		std::string get_name() {
			return _name;
		}

		// Get display info string for shader.
		std::string get_info_str() {
			return std::to_string(_program) + ":" + _name;
		}

		bool is_valid() const {
			return _pipeline != nullptr;
		}

		// The compiled pipeline state, for the renderer.
		MTL::RenderPipelineState *get_pipeline() const {
			return _pipeline;
		}

	private:
		// Our pipeline state, the Metal equivalent of a linked program.
		MTL::RenderPipelineState *_pipeline = nullptr;

		// Entry points selected by add_from_file().
		MTL::Function *_vertex_fn = nullptr;
		MTL::Function *_fragment_fn = nullptr;

		// Base names ("phong") of the requested stages, before metallib lookup.
		std::string _vertex_base;
		std::string _fragment_base;
		bool _has_geometry_stage = false;

		// Program id. Not a GL name any more, just a stable handle for logging
		// and for the Lua-visible get_program().
		GLuint _program = ShaderDefs::INVALID_SHADER;

		// Name for this shader.
		std::string _name;

		// Uniform name -> index into _uniform_members.
		std::unordered_map<std::string, GLuint> _uniforms;
		std::vector<uniform_member> _uniform_members;

		// CPU-side staging copy of the uniform block, uploaded per draw.
		std::vector<uint8_t> _uniform_data;

		// Constant vertex attribute values set via glVertexAttrib*.
		std::unordered_map<GLuint, glm::vec4> _static_attribs;

		// Texture/sampler argument names reported by reflection. These were
		// plain uniforms in GLSL but are separate arguments in Metal, so
		// set_uniform() on them is a no-op rather than a missing uniform.
		std::unordered_set<std::string> _texture_names;

		// Names already reported as missing, so a per-frame set_uniform() call
		// does not spam the log.
		std::unordered_set<std::string> _warned_uniforms;
		void warn_missing_uniform(const std::string &name);

		// Look up an entry point in the shared metallib.
		MTL::Function *lookup_function(const std::string &fn_name);
		// Build the vertex descriptor from the vertex function's declared
		// attributes.
		MTL::VertexDescriptor *build_vertex_descriptor();
		// Walk a reflected struct and record every member, recursing into nested
		// structs so light members come out as "u_ambient.color".
		void reflect_struct(MTL::StructType *type, const std::string &prefix, uint32_t base_offset);

		// Write raw bytes at a uniform's offset.
		void write_uniform(GLuint location, const void *data, size_t size);
};
