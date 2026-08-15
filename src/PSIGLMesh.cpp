#include "PSIGLMesh.h"
#include "PSIMetalContext.h"
#include "PSIGLShader.h"

#include <cstring>
#include <unordered_set>

namespace {

// GL primitive enum -> Metal primitive type.
//
// The GL values are part of the scripting contract: set_draw_mode() is bound to
// Lua and normal_vis.lua passes a bare 0 meaning GL_POINTS. So the numbers are
// translated here rather than changed at the source.
bool primitive_type_for(GLuint draw_mode, MTL::PrimitiveType *out) {
	switch (draw_mode) {
	case GL_POINTS:         *out = MTL::PrimitiveTypePoint;         return true;
	case GL_LINES:          *out = MTL::PrimitiveTypeLine;          return true;
	case GL_LINE_STRIP:     *out = MTL::PrimitiveTypeLineStrip;     return true;
	case GL_TRIANGLES:      *out = MTL::PrimitiveTypeTriangle;      return true;
	case GL_TRIANGLE_STRIP: *out = MTL::PrimitiveTypeTriangleStrip; return true;

	case GL_LINE_LOOP:
		// Metal has no line loop. A line strip draws every segment except the
		// closing one, which is the closest available behaviour.
		*out = MTL::PrimitiveTypeLineStrip;
		return true;

	case GL_TRIANGLE_FAN:
		// No Metal equivalent at all. Would need the index buffer rewritten as
		// a triangle list at mesh build time.
		return false;

	default:
		return false;
	}
}

// psilog_err() concatenates its format with a string literal, so it needs the
// message pre-built rather than a runtime format string.
void warn_unsupported_mode_once(const std::string &key, GLuint draw_mode) {
	static std::unordered_set<std::string> warned;
	if (warned.insert(key).second) {
		psilog_err("Unsupported draw mode %d on Metal "
		           "(GL_TRIANGLE_FAN has no equivalent and needs index conversion)",
		           draw_mode);
	}
}

} // namespace

PSIGLMesh::PSIGLMesh(GLuint vao, GLsizei draw_count, GLuint draw_mode) :
		_draw_count(draw_count), _draw_mode(draw_mode) {
	(void)vao;
}

PSIGLMesh::~PSIGLMesh() {
	for (GLuint i = 0; i <= BufferName::BufferName_MAX; i++) {
		if (_buffers[i] != nullptr) {
			_buffers[i]->release();
			_buffers[i] = nullptr;
		}
	}
}

bool PSIGLMesh::init() {
	gen_vao();
	gen_buffers();
	return true;
}

// Metal has no vertex array objects: the vertex layout lives in the pipeline
// state and the buffers are bound per draw. Kept as a no-op so create_gl_mesh()
// reads the same as before.
void PSIGLMesh::gen_vao() {
}

// Buffers are created on first upload, since Metal needs the size up front.
void PSIGLMesh::gen_buffers() {
}

void PSIGLMesh::bind_vao() {
}

GLuint PSIGLMesh::bound_name_for(GLenum target) const {
	return (target == GL_ELEMENT_ARRAY_BUFFER) ? _bound_index_name : _bound_array_name;
}

void PSIGLMesh::bind_buffer(GLenum target, GLuint buffer_name_id) {
	if (buffer_name_id > BufferName::BufferName_MAX) {
		return;
	}

	if (target == GL_ELEMENT_ARRAY_BUFFER) {
		_bound_index_name = buffer_name_id;
	} else {
		_bound_array_name = buffer_name_id;
	}
}

void PSIGLMesh::buffer_data(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage) {
	(void)usage;

	if (PSI_G::metal_ctx == nullptr || PSI_G::metal_ctx->device() == nullptr) {
		return;
	}
	if (size <= 0) {
		return;
	}

	GLuint name = bound_name_for(target);
	if (name > BufferName::BufferName_MAX) {
		return;
	}

	if (_buffers[name] != nullptr) {
		_buffers[name]->release();
		_buffers[name] = nullptr;
	}

	// Shared storage: this is an Apple-silicon-first engine and the CPU updates
	// vertex colours in place (PSIRenderObj::draw on material change), so a
	// private buffer plus a blit would cost more than it saves.
	MTL::ResourceOptions options = MTL::ResourceStorageModeShared;

	if (data != nullptr) {
		_buffers[name] = PSI_G::metal_ctx->device()->newBuffer(data, size, options);
	} else {
		_buffers[name] = PSI_G::metal_ctx->device()->newBuffer(size, options);
	}

	if (_buffers[name] == nullptr) {
		psilog_err("Failed allocating %ld byte buffer for attribute %d", (long)size, name);
	}
}

void PSIGLMesh::buffer_sub_data(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data) {
	GLuint name = bound_name_for(target);
	if (name > BufferName::BufferName_MAX || _buffers[name] == nullptr || data == nullptr) {
		return;
	}

	if (offset < 0 || (NS::UInteger)(offset + size) > _buffers[name]->length()) {
		psilog_err("buffer_sub_data out of range for attribute %d", name);
		return;
	}

	uint8_t *dst = static_cast<uint8_t *>(_buffers[name]->contents());
	std::memcpy(dst + offset, data, size);
}

void PSIGLMesh::enable_vertex_attrib(GLuint location, GLint size, GLsizei stride,
                                     const GLvoid *pointer, GLenum type) {
	(void)size;
	(void)stride;
	(void)pointer;
	(void)type;

	// The format and stride now come from the pipeline's vertex descriptor,
	// which PSIGLShader builds from what the vertex function declares. All that
	// is needed here is to record that this attribute has data.
	if (location <= BufferName::BufferName_MAX) {
		_attrib_enabled[location] = true;
	}
}

GLuint PSIGLMesh::enable_vertex_attrib(GLuint program, const GLchar *name,
                                       GLint size, GLsizei stride,
                                       const GLvoid *pointer, GLenum type) {
	(void)program;
	(void)name;

	// Callers bind the attribute's buffer immediately before this, and the
	// buffer name is the attribute index by convention (BufferName ==
	// AttribLocation == [[attribute(n)]]), so no name lookup is needed. The GL
	// version called glGetAttribLocation here.
	GLuint location = _bound_array_name;
	enable_vertex_attrib(location, size, stride, pointer, type);

	return location;
}

GLuint PSIGLMesh::get_buffer_id(GLuint buffer_name_id) {
	if (buffer_name_id > BufferName::BufferName_MAX || _buffers[buffer_name_id] == nullptr) {
		return 0;
	}
	// Not a GL name any more; callers only test it for validity and print it.
	return buffer_name_id + 1;
}

// Push the bound shader's staged uniform block before drawing.
//
// This lives at the draw call rather than in one caller because the draw paths
// are spread across PSIRenderObj::draw_mesh(), PSITextRenderer::draw() and
// Poly -- and under OpenGL every one of them got uniform updates applied
// immediately, with no flush step to forget.
void PSIGLMesh::flush_current_uniforms() {
	if (PSI_G::metal_ctx == nullptr) {
		return;
	}

	PSIGLShader *shader = PSI_G::metal_ctx->current_shader();
	if (shader != nullptr) {
		shader->bind_uniforms();
	}
}

void PSIGLMesh::bind_vertex_buffers(MTL::RenderCommandEncoder *encoder) {
	for (GLuint i = 0; i < BufferName::INDEX; i++) {
		if (_buffers[i] == nullptr || !_attrib_enabled[i]) {
			continue;
		}
		encoder->setVertexBuffer(_buffers[i], 0, i);
	}
}

void PSIGLMesh::draw() {
	if (PSI_G::metal_ctx == nullptr) {
		return;
	}

	MTL::RenderCommandEncoder *encoder = PSI_G::metal_ctx->encoder();
	if (encoder == nullptr || _draw_count <= 0) {
		return;
	}

	MTL::PrimitiveType prim;
	if (!primitive_type_for(_draw_mode, &prim)) {
		warn_unsupported_mode_once("draw:" + std::to_string(_draw_mode), _draw_mode);
		return;
	}

	flush_current_uniforms();
	bind_vertex_buffers(encoder);
	encoder->drawPrimitives(prim, (NS::UInteger)0, (NS::UInteger)_draw_count);
}

void PSIGLMesh::draw_instanced(GLuint vertex_count, GLuint instances) {
	if (PSI_G::metal_ctx == nullptr) {
		return;
	}

	MTL::RenderCommandEncoder *encoder = PSI_G::metal_ctx->encoder();
	if (encoder == nullptr || vertex_count == 0 || instances == 0) {
		return;
	}

	MTL::PrimitiveType prim;
	if (!primitive_type_for(_draw_mode, &prim)) {
		return;
	}

	flush_current_uniforms();
	bind_vertex_buffers(encoder);
	encoder->drawPrimitives(prim, (NS::UInteger)0, (NS::UInteger)vertex_count,
	                        (NS::UInteger)instances);
}

void PSIGLMesh::draw_indexed() {
	draw_indexed(0, _draw_count);
}

void PSIGLMesh::draw_indexed(GLuint offset, GLuint count) {
	if (PSI_G::metal_ctx == nullptr) {
		return;
	}

	MTL::RenderCommandEncoder *encoder = PSI_G::metal_ctx->encoder();
	if (encoder == nullptr || count == 0) {
		return;
	}

	MTL::Buffer *index_buffer = _buffers[BufferName::INDEX];
	if (index_buffer == nullptr) {
		return;
	}

	MTL::PrimitiveType prim;
	if (!primitive_type_for(_draw_mode, &prim)) {
		warn_unsupported_mode_once("draw_indexed:" + std::to_string(_draw_mode), _draw_mode);
		return;
	}

	MTL::IndexType index_type = (_index_type == GL_UNSIGNED_SHORT)
		? MTL::IndexTypeUInt16
		: MTL::IndexTypeUInt32;
	NS::UInteger index_size = (index_type == MTL::IndexTypeUInt16) ? 2 : 4;

	// Clamp the range to the buffer.
	//
	// PSITextRenderer's per-glyph reveal walks one glyph past the end of the
	// index buffer at the end of its animation (text.lua drives _draw_offset up
	// to the full string length, and draw() then asks for 6 more indices).
	// OpenGL read past the end silently; Metal's validation layer aborts the
	// process. Clamping keeps the old visual behaviour without the crash.
	NS::UInteger available = index_buffer->length() / index_size;
	if ((NS::UInteger)offset >= available) {
		return;
	}
	NS::UInteger draw_count = count;
	if ((NS::UInteger)offset + draw_count > available) {
		draw_count = available - offset;
	}
	if (draw_count == 0) {
		return;
	}

	flush_current_uniforms();
	bind_vertex_buffers(encoder);
	encoder->drawIndexedPrimitives(prim,
	                               draw_count,
	                               index_type,
	                               index_buffer,
	                               (NS::UInteger)offset * index_size);
}
