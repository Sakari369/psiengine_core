#include "PSIGLShader.h"
#include "PSIMetalContext.h"

#include <cstring>

namespace {

// Program ids are no longer GL names, but Lua can read them back through
// get_program() and the logs print them, so keep handing out stable numbers.
GLuint next_program_id() {
	static GLuint counter = 0;
	return ++counter;
}

// "…/shaders/phong.vert" -> "phong"
std::string base_name_from_path(const std::string &path) {
	size_t slash = path.find_last_of('/');
	std::string file = (slash == std::string::npos) ? path : path.substr(slash + 1);

	size_t dot = file.find_last_of('.');
	if (dot != std::string::npos) {
		file = file.substr(0, dot);
	}

	return file;
}

// Size of a Metal scalar/vector/matrix type as laid out in a vertex buffer.
// Vertex data is tightly packed on the CPU side (an array of glm::vec3 has a
// 12-byte stride), which matches MTLVertexFormatFloat3 exactly.
uint32_t vertex_attrib_size(MTL::DataType type) {
	switch (type) {
	case MTL::DataTypeFloat:  return 4;
	case MTL::DataTypeFloat2: return 8;
	case MTL::DataTypeFloat3: return 12;
	case MTL::DataTypeFloat4: return 16;
	case MTL::DataTypeInt:    return 4;
	case MTL::DataTypeUInt:   return 4;
	default:                  return 0;
	}
}

MTL::VertexFormat vertex_format_for(MTL::DataType type) {
	switch (type) {
	case MTL::DataTypeFloat:  return MTL::VertexFormatFloat;
	case MTL::DataTypeFloat2: return MTL::VertexFormatFloat2;
	case MTL::DataTypeFloat3: return MTL::VertexFormatFloat3;
	case MTL::DataTypeFloat4: return MTL::VertexFormatFloat4;
	case MTL::DataTypeInt:    return MTL::VertexFormatInt;
	case MTL::DataTypeUInt:   return MTL::VertexFormatUInt;
	default:                  return MTL::VertexFormatInvalid;
	}
}

} // namespace

PSIGLShader::~PSIGLShader() {
	if (_pipeline != nullptr) {
		_pipeline->release();
		_pipeline = nullptr;
	}
	if (_pipeline_opaque != nullptr) {
		_pipeline_opaque->release();
		_pipeline_opaque = nullptr;
	}
	if (_vertex_fn != nullptr) {
		_vertex_fn->release();
		_vertex_fn = nullptr;
	}
	if (_fragment_fn != nullptr) {
		_fragment_fn->release();
		_fragment_fn = nullptr;
	}
}

void PSIGLShader::warn_missing_uniform(const std::string &name) {
	// Texture samplers were plain uniforms in GLSL -- the draw path still does
	// set_uniform("u_diffuse", 0) to pick a texture unit. In Metal a texture is a
	// separate argument bound by PSIGLTexture::bind(), so there is no block
	// member by that name and nothing to do. Not an error.
	if (_texture_names.count(name) > 0) {
		return;
	}

	// set_uniform() runs per object per frame, so only complain once per name.
	if (_warned_uniforms.insert(name).second) {
		psilog_err("Shader %s: no uniform \"%s\" (add it to PSIUniforms in assets/shaders/psi_common.h)",
		           get_info_str().c_str(), name.c_str());
	}
}

MTL::Function *PSIGLShader::lookup_function(const std::string &fn_name) {
	if (PSI_G::metal_ctx == nullptr) {
		psilog_err("No Metal context when looking up shader function \"%s\"", fn_name.c_str());
		return nullptr;
	}

	MTL::Library *library = PSI_G::metal_ctx->shader_library();
	if (library == nullptr) {
		return nullptr;
	}

	NS::String *ns_name = NS::String::string(fn_name.c_str(), NS::UTF8StringEncoding);
	MTL::Function *fn = library->newFunction(ns_name);
	if (fn == nullptr) {
		psilog_err("Shader function \"%s\" not found in psishaders.metallib", fn_name.c_str());
	}

	return fn;
}

bool PSIGLShader::add_from_file(ShaderType type, std::string shader_path) {
	std::string base = base_name_from_path(shader_path);

	switch (type) {
	case ShaderType::VERTEX:
		_vertex_base = base;
		return true;

	case ShaderType::FRAGMENT:
		_fragment_base = base;
		return true;

	case ShaderType::GEOMETRY:
		// Metal has no geometry shaders. These are reimplemented as instanced
		// vertex shaders, so record the request and let compile() pick the
		// "_instanced" vertex variant instead of failing -- scripts like
		// poly_with_axis.lua and normal_vis.lua must keep loading.
		_has_geometry_stage = true;
		psilog(PSILog::OPENGL,
		       "Geometry stage \"%s\" requested; using instanced vertex path",
		       base.c_str());
		return true;
	}

	return false;
}

bool PSIGLShader::add_from_string(ShaderType type, std::string shader_str) {
	(void)shader_str;

	// The GLSL source itself is unusable: shaders are precompiled into
	// psishaders.metallib and Metal cannot consume GLSL at any point.
	//
	// The shader's name still identifies it, though. psi.shader.create_from_strings()
	// sets the name before adding the sources (assets/scripts/psi/shader.lua:104-105),
	// so an equivalent .metal shader can be resolved by that name -- which is how
	// plane_wave.lua, the only script that inlines shader source, keeps working
	// unedited.
	if (_name.empty()) {
		psilog_err("Shader %s: add_from_string() needs set_name() first so the "
		           "matching .metal shader can be found",
		           get_info_str().c_str());
		return false;
	}

	switch (type) {
	case ShaderType::VERTEX:
		_vertex_base = _name;
		break;
	case ShaderType::FRAGMENT:
		_fragment_base = _name;
		break;
	case ShaderType::GEOMETRY:
		_has_geometry_stage = true;
		break;
	}

	psilog(PSILog::OPENGL,
	       "Shader %s: inline GLSL source ignored; resolving \"%s\" from psishaders.metallib",
	       get_info_str().c_str(), _name.c_str());

	return true;
}

GLuint PSIGLShader::create_program() {
	if (_program == (GLuint)ShaderDefs::INVALID_SHADER) {
		_program = next_program_id();
	}
	return _program;
}

MTL::VertexDescriptor *PSIGLShader::build_vertex_descriptor() {
	MTL::VertexDescriptor *desc = MTL::VertexDescriptor::alloc()->init();

	// Build the layout from what the vertex function actually declares. Doing it
	// the other way round -- describing all seven attribute slots up front --
	// would reference vertex buffers that a given mesh never binds.
	NS::Array *attribs = _vertex_fn->vertexAttributes();
	if (attribs == nullptr) {
		return desc;
	}

	for (NS::UInteger i = 0; i < attribs->count(); i++) {
		MTL::VertexAttribute *attrib =
			static_cast<MTL::VertexAttribute *>(attribs->object(i));
		if (attrib == nullptr || !attrib->active()) {
			continue;
		}

		NS::UInteger index = attrib->attributeIndex();
		MTL::DataType type = attrib->attributeType();

		MTL::VertexFormat format = vertex_format_for(type);
		uint32_t stride = vertex_attrib_size(type);
		if (format == MTL::VertexFormatInvalid || stride == 0) {
			psilog_err("Shader %s: unsupported vertex attribute type at index %lu",
			           get_info_str().c_str(), (unsigned long)index);
			continue;
		}

		// One buffer per attribute, bound at the attribute's own index. This
		// mirrors PSIGLMesh, which keeps a separate VBO per attribute rather
		// than interleaving.
		desc->attributes()->object(index)->setFormat(format);
		desc->attributes()->object(index)->setOffset(0);
		desc->attributes()->object(index)->setBufferIndex(index);

		desc->layouts()->object(index)->setStride(stride);
		desc->layouts()->object(index)->setStepFunction(MTL::VertexStepFunctionPerVertex);
		desc->layouts()->object(index)->setStepRate(1);
	}

	return desc;
}

GLuint PSIGLShader::compile() {
	if (PSI_G::metal_ctx == nullptr || PSI_G::metal_ctx->device() == nullptr) {
		psilog_err("Shader %s: no Metal device", get_info_str().c_str());
		return ShaderDefs::LINK_FAILED;
	}

	create_program();

	// A program with no fragment stage was a transform-feedback capture program:
	// polyform.vert + polyform.geom amplified a point into a ring and the result
	// was captured to a buffer with the rasterizer discarded (see
	// poly_with_axis.lua:123-135).
	//
	// Metal has neither transform feedback nor geometry shaders, and the ring is
	// now generated directly in vertex_feedback, so there is nothing for this
	// program to do. Report it and succeed rather than failing -- the scripts
	// still create it, set it as the line material's shader, and call
	// add_uniforms() on it.
	if (_fragment_base.empty()) {
		psilog(PSILog::OPENGL,
		       "Shader %s: capture-only program (no fragment stage); "
		       "geometry is generated in the render pass instead",
		       get_info_str().c_str());
		return _program;
	}

	// The "_instanced" suffix marks a vertex entry point driven by
	// instance_id/vertex_id rather than one draw per object. Two things ask for
	// it, and they are disjoint in practice:
	//
	//   - a geometry stage, whose GLSL amplified points into triangles and is
	//     replaced by a vertex shader generating them from vertex_id;
	//   - set_instanced(), where the mesh carries a per-instance buffer.
	//
	// The fragment stage keeps its plain name either way.
	std::string vertex_fn_name = "vertex_" + _vertex_base;
	if (_has_geometry_stage || _instanced) {
		vertex_fn_name += "_instanced";
	}

	_vertex_fn = lookup_function(vertex_fn_name);
	if (_vertex_fn == nullptr) {
		return ShaderDefs::COMPILE_FAILED;
	}

	_fragment_fn = lookup_function("fragment_" + _fragment_base);
	if (_fragment_fn == nullptr) {
		return ShaderDefs::COMPILE_FAILED;
	}

	MTL::RenderPipelineDescriptor *pipeline_desc =
		MTL::RenderPipelineDescriptor::alloc()->init();

	pipeline_desc->setVertexFunction(_vertex_fn);
	pipeline_desc->setFragmentFunction(_fragment_fn);

	MTL::VertexDescriptor *vertex_desc = build_vertex_descriptor();
	pipeline_desc->setVertexDescriptor(vertex_desc);

	MTL::RenderPipelineColorAttachmentDescriptor *color =
		pipeline_desc->colorAttachments()->object(0);
	color->setPixelFormat(PSI_G::metal_ctx->color_format());

	// The GL renderer enabled GL_BLEND with SRC_ALPHA / ONE_MINUS_SRC_ALPHA for
	// the whole frame. In Metal blending is baked into the pipeline, so this
	// builds the blended variant first and the opaque one from the same
	// descriptor below.
	color->setBlendingEnabled(true);
	color->setRgbBlendOperation(MTL::BlendOperationAdd);
	color->setAlphaBlendOperation(MTL::BlendOperationAdd);
	color->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
	color->setSourceAlphaBlendFactor(MTL::BlendFactorSourceAlpha);
	color->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
	color->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);

	pipeline_desc->setDepthAttachmentPixelFormat(PSI_G::metal_ctx->depth_format());

	// Must match the render pass's sample count exactly, or pipeline creation
	// fails. This is why MSAA has to be decided before any shader compiles.
	pipeline_desc->setSampleCount(
		static_cast<NS::UInteger>(PSI_G::metal_ctx->get_msaa_samples()));

	NS::Error *error = nullptr;
	MTL::AutoreleasedRenderPipelineReflection reflection = nullptr;

	_pipeline = PSI_G::metal_ctx->device()->newRenderPipelineState(
		pipeline_desc,
		MTL::PipelineOptionArgumentInfo | MTL::PipelineOptionBufferTypeInfo,
		&reflection,
		&error);

	if (_pipeline == nullptr) {
		const char *msg = "unknown error";
		if (error != nullptr && error->localizedDescription() != nullptr) {
			msg = error->localizedDescription()->utf8String();
		}
		psilog_err("Shader %s: pipeline creation failed: %s", get_info_str().c_str(), msg);
		vertex_desc->release();
		pipeline_desc->release();
		return ShaderDefs::LINK_FAILED;
	}

	// Reflect straight away: the reflection object is autoreleased and only
	// valid here, and add_uniforms() is called separately by the scripts.
	_uniforms.clear();
	_uniform_members.clear();
	_fragment_has_uniforms = false;

	if (reflection != nullptr) {
		NS::Array *args = reflection->vertexBindings();
		for (NS::UInteger stage = 0; stage < 2; stage++) {
			args = (stage == 0) ? reflection->vertexBindings()
			                    : reflection->fragmentBindings();
			if (args == nullptr) {
				continue;
			}

			for (NS::UInteger i = 0; i < args->count(); i++) {
				MTL::Binding *base = static_cast<MTL::Binding *>(args->object(i));
				if (base == nullptr) {
					continue;
				}

				// Record texture arguments so set_uniform("u_diffuse", 0) --
				// which GLSL needed to pick a texture unit -- is recognised
				// rather than reported as a missing uniform.
				if (base->type() == MTL::BindingTypeTexture) {
					if (base->name() != nullptr) {
						_texture_names.insert(base->name()->utf8String());
					}
					continue;
				}

				if (base->type() != MTL::BindingTypeBuffer) {
					continue;
				}

				MTL::BufferBinding *binding = static_cast<MTL::BufferBinding *>(base);
				if (binding->index() != PSIMetal::BUFFER_UNIFORMS_VERTEX) {
					continue;
				}

				MTL::StructType *st = binding->bufferStructType();
				if (st == nullptr) {
					continue;
				}

				// stage 1 is the fragment function. Recording that it asked for
				// the block is what lets bind_uniforms() skip the second push
				// for shaders that never declared it.
				if (stage == 1) {
					_fragment_has_uniforms = true;
				}

				// Both stages bind the same struct at the same index, so the
				// second pass just re-resolves identical names.
				reflect_struct(st, "", 0);

				if (_uniform_data.size() < binding->bufferDataSize()) {
					_uniform_data.assign(binding->bufferDataSize(), 0);
				}
			}
		}
	}

	// The same pipeline with blending switched off, for materials that do not
	// need it. Everything else about the descriptor -- functions, vertex layout,
	// pixel formats, sample count -- is identical, so this is one extra
	// newRenderPipelineState per shader at load time and nothing at draw time.
	//
	// No reflection: the two share every binding, and the block was already
	// resolved above.
	color->setBlendingEnabled(false);
	NS::Error *opaque_error = nullptr;
	_pipeline_opaque = PSI_G::metal_ctx->device()->newRenderPipelineState(
		pipeline_desc, &opaque_error);

	if (_pipeline_opaque == nullptr) {
		// Not fatal: use_program() falls back to the blended pipeline, which is
		// what every object used before this existed.
		const char *msg = "unknown error";
		if (opaque_error != nullptr && opaque_error->localizedDescription() != nullptr) {
			msg = opaque_error->localizedDescription()->utf8String();
		}
		psilog_err("Shader %s: opaque pipeline variant failed (%s); "
		           "objects using it stay blended", get_info_str().c_str(), msg);
	}

	vertex_desc->release();
	pipeline_desc->release();

	resolve_hot_uniforms();

	psilog(PSILog::OPENGL, "Shader %s compiled (%s / %s), %zu uniforms, block %zu bytes",
	       get_info_str().c_str(), vertex_fn_name.c_str(),
	       ("fragment_" + _fragment_base).c_str(),
	       _uniform_members.size(), _uniform_data.size());

	return _program;
}

void PSIGLShader::reflect_struct(MTL::StructType *type, const std::string &prefix,
                                 uint32_t base_offset) {
	NS::Array *members = type->members();
	if (members == nullptr) {
		return;
	}

	for (NS::UInteger i = 0; i < members->count(); i++) {
		MTL::StructMember *member = static_cast<MTL::StructMember *>(members->object(i));
		if (member == nullptr) {
			continue;
		}

		std::string name = prefix + member->name()->utf8String();
		uint32_t offset = base_offset + static_cast<uint32_t>(member->offset());

		MTL::StructType *nested = member->structType();
		if (nested != nullptr) {
			// Nested struct: recurse so light members come out dotted, matching
			// the "u_ambient.color" names GL reported and setup_lights() uses.
			reflect_struct(nested, name + ".", offset);
			continue;
		}

		if (_uniforms.count(name) > 0) {
			continue;
		}

		uniform_member entry;
		entry.name = name;
		entry.offset = offset;
		entry.data_type = static_cast<uint32_t>(member->dataType());

		_uniforms[name] = static_cast<GLuint>(_uniform_members.size());
		_uniform_members.push_back(entry);
	}
}

void PSIGLShader::resolve_hot_uniforms() {
	// One hash lookup each, once per shader, replacing several per draw call.
	// Names come from the single shared PSIUniforms struct in
	// assets/shaders/psi_common.h; a shader that omits one keeps
	// INVALID_UNIFORM here and writing to it is a no-op, same as before.
	_hot.mvp_matrix        = get_uniform("u_model_view_projection_matrix");
	_hot.model_matrix      = get_uniform("u_model_matrix");
	_hot.view_matrix       = get_uniform("u_view_matrix");
	_hot.projection_matrix = get_uniform("u_projection_matrix");
	_hot.normal_matrix     = get_uniform("u_normal_matrix");
	_hot.color             = get_uniform("u_color");
	_hot.elapsed_time      = get_uniform("u_elapsed_time");

	// Reflection reports nested struct members dotted, so these match the names
	// setup_lights() used to pass.
	_hot.ambient_color     = get_uniform("u_ambient.color");
	_hot.ambient_intensity = get_uniform("u_ambient.intensity");
	_hot.light_pos         = get_uniform("u_light.pos");
	_hot.light_color       = get_uniform("u_light.color");
	_hot.light_intensity   = get_uniform("u_light.intensity");
	_hot.light_dir         = get_uniform("u_light.dir");
}

GLuint PSIGLShader::add_uniform(std::string name) {
	return get_uniform(name);
}

GLuint PSIGLShader::add_uniforms() {
	// Reflection already ran in compile(); this stays so the Lua-side
	// load_shaders() sequence (compile -> add_uniforms) keeps working.
	return static_cast<GLuint>(_uniform_members.size());
}

void PSIGLShader::add_transform_feedback_varyings(std::vector<std::string> varyings,
                                                  GLboolean interleaved) {
	(void)interleaved;

	// Metal has no transform feedback. Poly's capture path is reimplemented with
	// a compute kernel writing to a buffer; until then, report rather than
	// silently doing nothing, since the caller believes it captured something.
	std::string names;
	for (const auto &v : varyings) {
		names += v + " ";
	}
	psilog_err("Shader %s: transform feedback varyings (%s) not supported on Metal yet",
	           get_info_str().c_str(), names.c_str());
}

void PSIGLShader::use_program(bool blended) {
	if (PSI_G::metal_ctx == nullptr) {
		return;
	}

	MTL::RenderCommandEncoder *encoder = PSI_G::metal_ctx->encoder();
	if (encoder == nullptr) {
		return;
	}

	if (_pipeline == nullptr) {
		// This shader failed to build. Unbind rather than returning early:
		// Metal keeps the last pipeline set on the encoder, so leaving it alone
		// would draw this object through the PREVIOUS shader's pipeline, with a
		// vertex layout that does not match its mesh. That renders garbage
		// instead of simply omitting the object.
		PSI_G::metal_ctx->set_current_shader(nullptr);

		if (_warned_no_pipeline == false) {
			_warned_no_pipeline = true;
			psilog_err("Shader %s has no pipeline; objects using it will not be drawn",
			           get_info_str().c_str());
		}
		return;
	}

	MTL::RenderPipelineState *pipeline =
		(!blended && _pipeline_opaque != nullptr) ? _pipeline_opaque : _pipeline;
	encoder->setRenderPipelineState(pipeline);

	// Become the "bound program", so the draw call can flush our staged
	// uniforms no matter which class issues it.
	PSI_G::metal_ctx->set_current_shader(this);
}

void PSIGLShader::bind_uniforms() {
	if (PSI_G::metal_ctx == nullptr || _uniform_data.empty()) {
		return;
	}

	MTL::RenderCommandEncoder *encoder = PSI_G::metal_ctx->encoder();
	if (encoder == nullptr) {
		return;
	}

	// Under 4 KB, so setVertex/FragmentBytes is the cheapest path -- no buffer
	// allocation and no manual ring buffer. Both stages get the same block,
	// which is what reproduces OpenGL's single uniform namespace.
	encoder->setVertexBytes(_uniform_data.data(), _uniform_data.size(),
	                        PSIMetal::BUFFER_UNIFORMS_VERTEX);

	// ...unless the fragment function never declared the block. Pushing it there
	// anyway is a full copy of the block per draw that nothing reads.
	if (_fragment_has_uniforms) {
		encoder->setFragmentBytes(_uniform_data.data(), _uniform_data.size(),
		                          PSIMetal::BUFFER_UNIFORMS_VERTEX);
	}
}

void PSIGLShader::write_uniform(GLuint location, const void *data, size_t size) {
	if (location >= _uniform_members.size()) {
		return;
	}

	uint32_t offset = _uniform_members[location].offset;
	if (offset + size > _uniform_data.size()) {
		psilog_err("Shader %s: uniform \"%s\" writes past the block (%u + %zu > %zu)",
		           get_info_str().c_str(), _uniform_members[location].name.c_str(),
		           offset, size, _uniform_data.size());
		return;
	}

	std::memcpy(_uniform_data.data() + offset, data, size);
}

void PSIGLShader::set_uniform(GLuint location, const GLint &val) {
	if (location >= _uniform_members.size()) {
		return;
	}

	// Poly.cpp:148 sets u_num_points as an int although the shader declares it
	// float. GL silently rejected the mismatch; convert instead.
	if (_uniform_members[location].data_type == MTL::DataTypeFloat) {
		GLfloat f = static_cast<GLfloat>(val);
		write_uniform(location, &f, sizeof(f));
		return;
	}

	write_uniform(location, &val, sizeof(val));
}

void PSIGLShader::set_uniform(GLuint location, const GLuint &val) {
	if (location >= _uniform_members.size()) {
		return;
	}

	if (_uniform_members[location].data_type == MTL::DataTypeFloat) {
		GLfloat f = static_cast<GLfloat>(val);
		write_uniform(location, &f, sizeof(f));
		return;
	}

	write_uniform(location, &val, sizeof(val));
}

void PSIGLShader::set_uniform(GLuint location, const GLfloat &val) {
	write_uniform(location, &val, sizeof(val));
}

void PSIGLShader::set_uniform(GLuint location, const glm::vec2 &vec) {
	write_uniform(location, glm::value_ptr(vec), sizeof(float) * 2);
}

void PSIGLShader::set_uniform(GLuint location, const glm::vec3 &vec) {
	// MSL float3 occupies 16 bytes but only the first 12 are the value, so
	// writing 12 leaves the pad alone.
	write_uniform(location, glm::value_ptr(vec), sizeof(float) * 3);
}

void PSIGLShader::set_uniform(GLuint location, const glm::vec4 &vec) {
	write_uniform(location, glm::value_ptr(vec), sizeof(float) * 4);
}

void PSIGLShader::set_uniform(GLuint location, const glm::mat3 &mat) {
	if (location >= _uniform_members.size()) {
		return;
	}

	// glm::mat3 is three tightly packed vec3 columns (36 bytes). MSL float3x3 is
	// three float3 columns on 16-byte alignment (48 bytes). Copy column by
	// column; a straight memcpy would shear the matrix.
	uint32_t offset = _uniform_members[location].offset;
	if (offset + 48 > _uniform_data.size()) {
		psilog_err("Shader %s: mat3 uniform \"%s\" writes past the block",
		           get_info_str().c_str(), _uniform_members[location].name.c_str());
		return;
	}

	uint8_t *dst = _uniform_data.data() + offset;
	for (int col = 0; col < 3; col++) {
		std::memcpy(dst + (col * 16), glm::value_ptr(mat[col]), sizeof(float) * 3);
	}
}

void PSIGLShader::set_uniform(GLuint location, const glm::mat4 &mat) {
	// glm::mat4 and MSL float4x4 have identical layout: 4 columns of 4 floats.
	write_uniform(location, glm::value_ptr(mat), sizeof(float) * 16);
}

void PSIGLShader::set_vertex_attrib(GLuint index, const glm::vec2 &vec) {
	_static_attribs[index] = glm::vec4(vec, 0.0f, 1.0f);
}

void PSIGLShader::set_vertex_attrib(GLuint index, const glm::vec3 &vec) {
	_static_attribs[index] = glm::vec4(vec, 1.0f);
}

void PSIGLShader::set_vertex_attrib(GLuint index, const glm::vec4 &vec) {
	_static_attribs[index] = vec;
}

bool PSIGLShader::has_static_vertex_attrib(GLuint index) const {
	return _static_attribs.count(index) > 0;
}

glm::vec4 PSIGLShader::get_static_vertex_attrib(GLuint index) const {
	auto it = _static_attribs.find(index);
	if (it == _static_attribs.end()) {
		return glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	}
	return it->second;
}
