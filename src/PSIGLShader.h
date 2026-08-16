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

		PSIGLShader();
		~PSIGLShader();

		static ShaderSharedPtr create() {
			return make_shared<PSIGLShader>();
		}

		// Every live shader, in creation order.
		//
		// Self-maintaining: a shader adds itself on construction and removes
		// itself on destruction. It has to work that way because scripts build
		// shaders with a bare PSIGLShader() through psi/shader.lua and never go
		// near PSIResourceManager, so there is no other place that sees them
		// all. PSIRenderPass::warm_pipelines() walks this.
		//
		// Raw pointers deliberately -- this must not keep a shader alive.
		static const std::vector<PSIGLShader *> &all();

		// The uniforms the draw path writes on every object, every frame.
		//
		// set_uniform(name, value) hashes an std::unordered_map key built from a
		// string literal at the call site. "u_model_view_projection_matrix" is 30
		// characters, past libc++'s 22-character short-string buffer, so each of
		// those calls was a malloc and a free -- per draw. These are resolved once
		// in compile() and the draw path uses the by-location overloads instead.
		//
		// Any member a given shader does not declare stays INVALID_UNIFORM, and
		// every by-location setter bounds-checks, so writing through one is a
		// silent no-op exactly as the name-based path was.
		struct hot_uniforms {
			GLuint mvp_matrix        = (GLuint)INVALID_UNIFORM;
			GLuint model_matrix      = (GLuint)INVALID_UNIFORM;
			GLuint view_matrix       = (GLuint)INVALID_UNIFORM;
			GLuint projection_matrix = (GLuint)INVALID_UNIFORM;
			GLuint normal_matrix     = (GLuint)INVALID_UNIFORM;
			GLuint color             = (GLuint)INVALID_UNIFORM;
			GLuint elapsed_time      = (GLuint)INVALID_UNIFORM;

			GLuint ambient_color     = (GLuint)INVALID_UNIFORM;
			GLuint ambient_intensity = (GLuint)INVALID_UNIFORM;
			GLuint light_pos         = (GLuint)INVALID_UNIFORM;
			GLuint light_color       = (GLuint)INVALID_UNIFORM;
			GLuint light_intensity   = (GLuint)INVALID_UNIFORM;
			GLuint light_dir         = (GLuint)INVALID_UNIFORM;
		};

		const hot_uniforms &hot() const {
			return _hot;
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
		//
		// Which pipeline that is depends on two things Metal bakes in: whether
		// blending is on, and the signature of the pass being encoded (see
		// PSIMetal::pass_signature). The matching variant is built on first use
		// and cached. Blended is the default because that is what the GL
		// renderer did globally, and it is the only safe answer without knowing
		// the material. See PSIGLMaterial::set_blending().
		void use_program(bool blended = true);

		// Build both blend variants for this pass signature up front.
		//
		// Pipeline creation costs milliseconds, so a pass that is first encoded
		// mid-animation would otherwise stutter on its opening frame. Passes
		// call this when they are created, which is setup time.
		void warm_pipelines(const PSIMetal::pass_signature &sig);

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
		// The u_params tuning channel; see psi_common.h. Named rather than
		// exposing set_uniform generically to Lua, because every other uniform
		// in the block is owned by the renderer and writing one from a script
		// would be fighting it.
		void set_params(const glm::vec4 &params) {
			set_uniform("u_params", params);
		}

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
		void add_transform_feedback_varyings(std::vector<std::string> varyings);

		// Select the metallib entry point for this stage from a GLSL file name.
		bool add_from_file(ShaderType type, std::string shader_path);
		// Legacy: the GLSL source is unusable, so this resolves by NAME instead.
		// See add_source() for source that is actually compiled.
		bool add_from_string(ShaderType type, std::string shader_str);

		// Compile Metal Shading Language at runtime, and resolve this shader's
		// entry points from the result.
		//
		// Everything else here comes out of psishaders.metallib, which the build
		// compiles ahead of time from assets/shaders/*.metal. That is the right
		// default -- compilation costs milliseconds and doing it at startup for
		// every shader would be waste -- but it means a shader has to exist as a
		// file in the tree before a script can use it. This is the escape hatch:
		// a script can carry its own shader, generate one, or build variants
		// from a template.
		//
		// Entry points still follow the naming convention, so a source string
		// added to a shader named "swirl" must define vertex_swirl and
		// fragment_swirl. Lookup checks this library first and falls back to the
		// metallib, so a script can override one stage and inherit the other.
		//
		// #include does not work at runtime -- there is no file system behind
		// the compiler -- so an `#include "psi_common.h"` line is substituted
		// with that file's contents before compiling, and the contents are
		// prepended if the line is absent. Source is therefore copy-pasteable
		// between a .metal file and a script either way.
		//
		// Returns false and logs the compiler's own diagnostics on failure.
		bool add_source(std::string source);

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

		// Build this shader against the instanced entry point, which reads
		// per-instance data from PSIMetal::BUFFER_INSTANCE_DATA. Must be called
		// before compile().
		void set_instanced(GLboolean instanced) {
			_instanced = instanced;
		}
		bool is_instanced() const {
			return _instanced;
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

		// Does this shader have a pipeline bound draws can go through?
		//
		// Not the same question as "did compile() succeed" -- a capture-only
		// program (polyform: a vertex stage and no fragment stage) compiles
		// successfully and deliberately has no pipeline. Use is_compiled() to
		// test for failure.
		bool is_valid() const {
			return !_pipelines.empty();
		}

		// Which slot does this shader bind the named texture at?
		//
		// -1 when the shader has no such texture, which a material treats as
		// "this shader does not want that map" rather than as an error -- the
		// same material can then be used with a shader that reads three maps
		// and one that reads one.
		GLint get_texture_slot(const std::string &name) const {
			auto it = _texture_slots.find(name);
			return (it == _texture_slots.end()) ? -1 : (GLint)it->second;
		}

		// The sampler that goes with it.
		//
		// Resolved by the "<texture>_sampler" convention every shader here
		// follows, falling back to the texture's own slot index -- which is
		// what the pairing was before samplers were reflected at all.
		GLint get_sampler_slot(const std::string &texture_name) const {
			auto it = _sampler_slots.find(texture_name + "_sampler");
			if (it != _sampler_slots.end()) {
				return (GLint)it->second;
			}
			return get_texture_slot(texture_name);
		}

		// Did compile() succeed?
		//
		// This exists because compile()'s return value cannot answer it. It
		// returns LINK_FAILED (0) or COMPILE_FAILED (1) on the way out, and
		// _program on success -- but _program is handed out by a counter that
		// starts at 1, so the first shader built in a process returns a success
		// value numerically equal to COMPILE_FAILED. The return type is GLuint
		// as well, so the INVALID_SHADER (-1) that every caller in
		// assets/scripts/psi/shader.lua tests against is doubly unreachable.
		//
		// The practical effect before this existed: a shader that failed to
		// build produced a live object, use_program() unbound the encoder and
		// logged once, and the object silently vanished from the scene with no
		// Lua-visible signal at all.
		bool is_compiled() const {
			return _compiled;
		}

	private:
		// One pipeline per (pass signature, blend) combination.
		//
		// Blending, the attachment formats and the sample count are all baked
		// into an MTLRenderPipelineState, so a shader used in two passes that
		// differ in any of them needs one object per combination. They are built
		// on demand and kept for the shader's lifetime; in practice a scene uses
		// two or three per shader.
		struct pipeline_key {
			uint32_t color_format;
			uint32_t depth_format;
			uint32_t sample_count;
			uint32_t blended;

			bool operator==(const pipeline_key &rhs) const {
				return color_format == rhs.color_format &&
				       depth_format == rhs.depth_format &&
				       sample_count == rhs.sample_count &&
				       blended == rhs.blended;
			}
		};

		struct pipeline_key_hash {
			size_t operator()(const pipeline_key &k) const {
				// Formats are small enums and the sample count is 1..8, so
				// shifting them into one word collides only across absurd
				// values.
				size_t h = k.color_format;
				h = h * 31 + k.depth_format;
				h = h * 31 + k.sample_count;
				h = h * 31 + k.blended;
				return h;
			}
		};

		static pipeline_key make_key(const PSIMetal::pass_signature &sig, bool blended) {
			return pipeline_key{
				(uint32_t)sig.color_format,
				(uint32_t)sig.depth_format,
				sig.sample_count,
				blended ? 1u : 0u,
			};
		}

		std::unordered_map<pipeline_key, MTL::RenderPipelineState *, pipeline_key_hash> _pipelines;

		// Look the variant up, building it on a miss. Null if it cannot be built.
		MTL::RenderPipelineState *pipeline_for(const PSIMetal::pass_signature &sig, bool blended);
		// Create one variant. reflection is non-null only for the first build,
		// which is where the uniform block is resolved.
		MTL::RenderPipelineState *build_pipeline(const PSIMetal::pass_signature &sig, bool blended,
		                                         MTL::AutoreleasedRenderPipelineReflection *reflection);

		// Vertex layout, derived from the vertex function's declared attributes.
		// Retained because every lazily built variant needs it again.
		MTL::VertexDescriptor *_vertex_desc = nullptr;

		// Library compiled from source at runtime; see add_source(). Null for
		// the usual case of a shader living in psishaders.metallib.
		MTL::Library *_runtime_library = nullptr;

		// Entry points selected by add_from_file().
		MTL::Function *_vertex_fn = nullptr;
		MTL::Function *_fragment_fn = nullptr;

		// Base names ("phong") of the requested stages, before metallib lookup.
		std::string _vertex_base;
		std::string _fragment_base;
		bool _has_geometry_stage = false;
		// Resolve to the "_instanced" vertex entry point, which reads the
		// per-instance buffer.
		bool _instanced = false;

		// Program id. Not a GL name any more, just a stable handle for logging
		// and for the Lua-visible get_program().
		GLuint _program = ShaderDefs::INVALID_SHADER;

		// Did compile() reach the end without failing? See is_compiled().
		bool _compiled = false;

		// Name for this shader.
		std::string _name;

		// Uniform name -> index into _uniform_members.
		std::unordered_map<std::string, GLuint> _uniforms;
		std::vector<uniform_member> _uniform_members;

		// Per-draw uniform locations, resolved once. See hot_uniforms.
		hot_uniforms _hot;
		void resolve_hot_uniforms();

		// Does the fragment function actually bind the uniform block?
		//
		// Both stages were handed the same block unconditionally, but reflection
		// already reports which of them asked for it -- fragment_text, for one,
		// does not declare PSIUniforms at all. Skipping the push for those halves
		// the uniform traffic of every draw that uses them.
		bool _fragment_has_uniforms = false;

		// CPU-side staging copy of the uniform block, uploaded per draw.
		std::vector<uint8_t> _uniform_data;

		// Constant vertex attribute values set via glVertexAttrib*.
		std::unordered_map<GLuint, glm::vec4> _static_attribs;

		// Texture/sampler argument names reported by reflection. These were
		// plain uniforms in GLSL but are separate arguments in Metal, so
		// set_uniform() on them is a no-op rather than a missing uniform.
		// Texture and sampler argument name -> the [[texture(n)]] /
		// [[sampler(n)]] slot reflection reported it at.
		//
		// These were plain uniforms in GLSL but are separate arguments in
		// Metal, so set_uniform() on one is a no-op rather than a missing
		// uniform. Keeping the index as well as the name is what lets a
		// material name its textures -- "u_diffuse", "u_normal" -- and have the
		// draw path bind each at the slot its own shader declared, instead of
		// everything going to slot 0.
		std::unordered_map<std::string, uint32_t> _texture_slots;
		std::unordered_map<std::string, uint32_t> _sampler_slots;

		// Names already reported as missing, so a per-frame set_uniform() call
		// does not spam the log.
		std::unordered_set<std::string> _warned_uniforms;

		// use_program() runs per frame; only report a missing pipeline once.
		bool _warned_no_pipeline = false;
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
