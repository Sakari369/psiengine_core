// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// TEMPORARY OpenGL compatibility shim for the Metal port. DELETE ME.
//
// GLEW is gone, but ~179 GL call sites across PSIGLRenderer/Shader/Mesh/Texture
// and Poly are still GL-shaped and get rewritten one milestone at a time. This
// header lets the whole project keep compiling and running in between, so each
// milestone can be built and tested instead of the tree staying dark until the
// last file is ported.
//
// Every function here is an inert no-op. Queries return "success" so existing
// error-checking paths stay quiet rather than aborting on a shim.
//
// The ENUM VALUES ARE REAL. That matters: Lua scripts pass raw GL enums as bare
// integers (normal_vis.lua calls set_draw_mode(0), meaning GL_POINTS), so these
// numbers are part of the scripting contract and outlive the function stubs.
//
// Removal plan -- delete each block as its owner is rewritten:
//   M1 PSIVideo        -> glew*, glViewport, GL_VIEWPORT/SAMPLES/EXTENSIONS
//   M2 PSIGLShader     -> gl*Shader/Program/Uniform*/TransformFeedbackVaryings
//   M3 PSIGLMesh       -> gl*Buffer*/VertexArray*/VertexAttrib*/glDraw*
//      PSIGLTexture    -> gl*Texture*/glTexImage*/glTexParameteri
//   M4/M6 Poly         -> transform feedback, stencil, rasterizer discard
//   M5 PSIGLRenderer   -> framebuffers, render state, glReadPixels

#pragma once

#include <cstddef>
#include "PSITypes.h"

// ---------------------------------------------------------------------------
// Enum values (real GL numbers -- see header comment)
// ---------------------------------------------------------------------------

// Booleans and errors.
#define GL_FALSE 0
#define GL_TRUE  1
#define GL_NO_ERROR                        0
#define GL_INVALID_ENUM                    0x0500
#define GL_INVALID_VALUE                   0x0501
#define GL_INVALID_OPERATION               0x0502
#define GL_OUT_OF_MEMORY                   0x0505
#define GL_INVALID_FRAMEBUFFER_OPERATION   0x0506

// Primitive modes. Lua passes these as bare integers -- do not renumber.
#define GL_POINTS         0x0000
#define GL_LINES          0x0001
#define GL_LINE_LOOP      0x0002
#define GL_LINE_STRIP     0x0003
#define GL_TRIANGLES      0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TRIANGLE_FAN   0x0006

// Scalar types.
#define GL_BYTE           0x1400
#define GL_UNSIGNED_BYTE  0x1401
#define GL_SHORT          0x1402
#define GL_UNSIGNED_SHORT 0x1403
#define GL_INT            0x1404
#define GL_UNSIGNED_INT   0x1405
#define GL_FLOAT          0x1406

// Buffers.
#define GL_ARRAY_BUFFER         0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW          0x88E4
#define GL_DYNAMIC_DRAW         0x88E8

// Pixel formats.
#define GL_DEPTH_COMPONENT   0x1902
#define GL_RED               0x1903
#define GL_RGB               0x1907
#define GL_RGBA              0x1908
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_SRGB8_ALPHA8      0x8C43

#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3

// Textures.
#define GL_TEXTURE_2D                  0x0DE1
#define GL_TEXTURE_CUBE_MAP            0x8513
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X 0x8515
#define GL_TEXTURE_2D_MULTISAMPLE      0x9100
#define GL_TEXTURE_CUBE_MAP_SEAMLESS   0x884F
#define GL_TEXTURE0                    0x84C0

#define GL_NEAREST               0x2600
#define GL_LINEAR                0x2601
#define GL_LINEAR_MIPMAP_LINEAR  0x2703
#define GL_REPEAT                0x2901
#define GL_CLAMP_TO_EDGE         0x812F
#define GL_CLAMP_TO_BORDER       0x812D

#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S     0x2802
#define GL_TEXTURE_WRAP_T     0x2803
#define GL_TEXTURE_WRAP_R     0x8072
#define GL_TEXTURE_BASE_LEVEL 0x813C
#define GL_TEXTURE_MAX_LEVEL  0x813D

#define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF

// glew.h defined this as a plain 1 (an "extension is known to the header" flag,
// not a runtime capability). PSIGLTexture.cpp:148,156,164 branch on it as if it
// were a runtime check, so those branches were always taken. Kept at 1 so the
// shim preserves current behaviour exactly. Goes away in M3 -- MTLSamplerState
// has maxAnisotropy natively.
#define GL_EXT_texture_filter_anisotropic 1

// Render state.
#define GL_DEPTH_TEST          0x0B71
#define GL_BLEND               0x0BE2
#define GL_CULL_FACE           0x0B44
#define GL_MULTISAMPLE         0x809D
#define GL_STENCIL_TEST        0x0B90
#define GL_RASTERIZER_DISCARD  0x8C89

#define GL_LESS   0x0201
#define GL_EQUAL  0x0202
#define GL_ALWAYS 0x0207

#define GL_SRC_ALPHA           0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303

#define GL_FRONT          0x0404
#define GL_BACK           0x0405
#define GL_FRONT_AND_BACK 0x0408

#define GL_FILL 0x1B02
#define GL_LINE 0x1B01

#define GL_KEEP    0x1E00
#define GL_REPLACE 0x1E01

#define GL_COLOR_BUFFER_BIT   0x00004000
#define GL_DEPTH_BUFFER_BIT   0x00000100
#define GL_STENCIL_BUFFER_BIT 0x00000400

// Framebuffers.
#define GL_FRAMEBUFFER          0x8D40
#define GL_RENDERBUFFER         0x8D41
#define GL_COLOR_ATTACHMENT0    0x8CE0
#define GL_DEPTH_ATTACHMENT     0x8D00
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5

// Shaders.
#define GL_VERTEX_SHADER   0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_GEOMETRY_SHADER 0x8DD9

#define GL_COMPILE_STATUS               0x8B81
#define GL_LINK_STATUS                  0x8B82
#define GL_INFO_LOG_LENGTH              0x8B84
#define GL_ACTIVE_UNIFORMS              0x8B86
#define GL_ACTIVE_UNIFORM_MAX_LENGTH    0x8B87
#define GL_ACTIVE_ATTRIBUTE_MAX_LENGTH  0x8B8A

// Transform feedback.
#define GL_TRANSFORM_FEEDBACK        0x8E22
#define GL_TRANSFORM_FEEDBACK_BUFFER 0x8C8E
#define GL_INTERLEAVED_ATTRIBS       0x8C8C
#define GL_SEPARATE_ATTRIBS          0x8C8D

// Geometry shader limits.
#define GL_GEOMETRY_VERTICES_OUT                 0x8916
#define GL_MAX_GEOMETRY_OUTPUT_VERTICES          0x8DE0
#define GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS  0x8DE1
#define GL_MAX_GEOMETRY_OUTPUT_COMPONENTS        0x9124

// Misc queries.
#define GL_VIEWPORT        0x0BA2
#define GL_SAMPLE_BUFFERS  0x80A8
#define GL_SAMPLES         0x80A9
#define GL_PACK_ALIGNMENT  0x0D05
#define GL_EXTENSIONS      0x1F03
#define GL_NUM_EXTENSIONS  0x821D

#define GLEW_OK 0

// ---------------------------------------------------------------------------
// Function stubs
// ---------------------------------------------------------------------------

namespace psi_glcompat {
// Hands out plausible non-zero object names so code that checks "did I get a
// valid id" keeps working while the real backend is built.
inline GLuint next_name() {
	static GLuint counter = 0;
	return ++counter;
}
inline void fill_names(GLsizei n, GLuint *out) {
	for (GLsizei i = 0; i < n; i++) {
		out[i] = next_name();
	}
}
} // namespace psi_glcompat

#define PSI_GL_STUB inline void

// Errors and strings.
inline GLenum glGetError() { return GL_NO_ERROR; }
inline const GLubyte *glGetStringi(GLenum, GLuint) { return nullptr; }

// GLEW.
inline GLboolean glewExperimental = false;
inline GLenum glewInit() { return GLEW_OK; }
inline const GLubyte *glewGetErrorString(GLenum) {
	return reinterpret_cast<const GLubyte *>("glew removed (Metal port)");
}

// Global state.
PSI_GL_STUB glEnable(GLenum) {}
PSI_GL_STUB glDisable(GLenum) {}
PSI_GL_STUB glViewport(GLint, GLint, GLsizei, GLsizei) {}
PSI_GL_STUB glClear(GLbitfield) {}
PSI_GL_STUB glClearColor(GLclampf, GLclampf, GLclampf, GLclampf) {}
PSI_GL_STUB glDepthFunc(GLenum) {}
PSI_GL_STUB glBlendFunc(GLenum, GLenum) {}
PSI_GL_STUB glCullFace(GLenum) {}
PSI_GL_STUB glPolygonMode(GLenum, GLenum) {}
PSI_GL_STUB glPixelStorei(GLenum, GLint) {}
PSI_GL_STUB glReadBuffer(GLenum) {}
PSI_GL_STUB glReadPixels(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, GLvoid *) {}
PSI_GL_STUB glDrawBuffers(GLsizei, const GLenum *) {}

inline void glGetIntegerv(GLenum, GLint *params) {
	if (params) {
		*params = 0;
	}
}

// Stencil.
PSI_GL_STUB glStencilFunc(GLenum, GLint, GLuint) {}
PSI_GL_STUB glStencilOp(GLenum, GLenum, GLenum) {}
PSI_GL_STUB glStencilMask(GLuint) {}

// Shaders and programs.
inline GLuint glCreateProgram() { return psi_glcompat::next_name(); }
inline GLuint glCreateShader(GLenum) { return psi_glcompat::next_name(); }
PSI_GL_STUB glShaderSource(GLuint, GLsizei, const GLchar *const *, const GLint *) {}
PSI_GL_STUB glCompileShader(GLuint) {}
PSI_GL_STUB glAttachShader(GLuint, GLuint) {}
PSI_GL_STUB glDetachShader(GLuint, GLuint) {}
PSI_GL_STUB glDeleteShader(GLuint) {}
PSI_GL_STUB glLinkProgram(GLuint) {}
PSI_GL_STUB glUseProgram(GLuint) {}
PSI_GL_STUB glTransformFeedbackVaryings(GLuint, GLsizei, const GLchar *const *, GLenum) {}

// Report success so existing compile/link error paths do not fire on a stub.
inline void glGetShaderiv(GLuint, GLenum pname, GLint *params) {
	if (!params) {
		return;
	}
	*params = (pname == GL_INFO_LOG_LENGTH) ? 0 : GL_TRUE;
}
inline void glGetProgramiv(GLuint, GLenum pname, GLint *params) {
	if (!params) {
		return;
	}
	switch (pname) {
	case GL_INFO_LOG_LENGTH:
	case GL_ACTIVE_UNIFORMS:
	case GL_ACTIVE_UNIFORM_MAX_LENGTH:
	case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
		*params = 0;
		break;
	default:
		*params = GL_TRUE;
		break;
	}
}
inline void glGetShaderInfoLog(GLuint, GLsizei, GLsizei *length, GLchar *info) {
	if (length) {
		*length = 0;
	}
	if (info) {
		info[0] = '\0';
	}
}
inline void glGetProgramInfoLog(GLuint, GLsizei, GLsizei *length, GLchar *info) {
	if (length) {
		*length = 0;
	}
	if (info) {
		info[0] = '\0';
	}
}
inline void glGetActiveUniform(GLuint, GLuint, GLsizei, GLsizei *length, GLint *size,
                               GLenum *type, GLchar *name) {
	if (length) { *length = 0; }
	if (size)   { *size = 0; }
	if (type)   { *type = GL_FLOAT; }
	if (name)   { name[0] = '\0'; }
}
inline void glGetActiveAttrib(GLuint, GLuint, GLsizei, GLsizei *length, GLint *size,
                              GLenum *type, GLchar *name) {
	if (length) { *length = 0; }
	if (size)   { *size = 0; }
	if (type)   { *type = GL_FLOAT; }
	if (name)   { name[0] = '\0'; }
}
inline GLint glGetUniformLocation(GLuint, const GLchar *) { return -1; }
inline GLint glGetAttribLocation(GLuint, const GLchar *) { return -1; }

// Uniforms.
PSI_GL_STUB glUniform1i(GLint, GLint) {}
PSI_GL_STUB glUniform1ui(GLint, GLuint) {}
PSI_GL_STUB glUniform1f(GLint, GLfloat) {}
PSI_GL_STUB glUniform2f(GLint, GLfloat, GLfloat) {}
PSI_GL_STUB glUniform3f(GLint, GLfloat, GLfloat, GLfloat) {}
PSI_GL_STUB glUniform4f(GLint, GLfloat, GLfloat, GLfloat, GLfloat) {}
PSI_GL_STUB glUniformMatrix3fv(GLint, GLsizei, GLboolean, const GLfloat *) {}
PSI_GL_STUB glUniformMatrix4fv(GLint, GLsizei, GLboolean, const GLfloat *) {}

// Vertex arrays, buffers and attributes.
inline void glGenVertexArrays(GLsizei n, GLuint *arrays) { psi_glcompat::fill_names(n, arrays); }
inline void glGenBuffers(GLsizei n, GLuint *buffers) { psi_glcompat::fill_names(n, buffers); }
PSI_GL_STUB glDeleteVertexArrays(GLsizei, const GLuint *) {}
PSI_GL_STUB glDeleteBuffers(GLsizei, const GLuint *) {}
PSI_GL_STUB glBindVertexArray(GLuint) {}
PSI_GL_STUB glBindBuffer(GLenum, GLuint) {}
PSI_GL_STUB glBindBufferBase(GLenum, GLuint, GLuint) {}
PSI_GL_STUB glBufferData(GLenum, GLsizeiptr, const GLvoid *, GLenum) {}
PSI_GL_STUB glBufferSubData(GLenum, GLintptr, GLsizeiptr, const GLvoid *) {}
PSI_GL_STUB glVertexAttribPointer(GLuint, GLint, GLenum, GLboolean, GLsizei, const GLvoid *) {}
PSI_GL_STUB glEnableVertexAttribArray(GLuint) {}
PSI_GL_STUB glVertexAttrib2f(GLuint, GLfloat, GLfloat) {}
PSI_GL_STUB glVertexAttrib3f(GLuint, GLfloat, GLfloat, GLfloat) {}
PSI_GL_STUB glVertexAttrib4f(GLuint, GLfloat, GLfloat, GLfloat, GLfloat) {}

// Draws.
PSI_GL_STUB glDrawArrays(GLenum, GLint, GLsizei) {}
PSI_GL_STUB glDrawElements(GLenum, GLsizei, GLenum, const GLvoid *) {}

// Transform feedback.
inline void glGenTransformFeedbacks(GLsizei n, GLuint *ids) { psi_glcompat::fill_names(n, ids); }
PSI_GL_STUB glBindTransformFeedback(GLenum, GLuint) {}
PSI_GL_STUB glBeginTransformFeedback(GLenum) {}
PSI_GL_STUB glEndTransformFeedback() {}
PSI_GL_STUB glDrawTransformFeedback(GLenum, GLuint) {}

// Textures.
inline void glGenTextures(GLsizei n, GLuint *textures) { psi_glcompat::fill_names(n, textures); }
PSI_GL_STUB glBindTexture(GLenum, GLuint) {}
PSI_GL_STUB glActiveTexture(GLenum) {}
PSI_GL_STUB glTexParameteri(GLenum, GLenum, GLint) {}
PSI_GL_STUB glGenerateMipmap(GLenum) {}
PSI_GL_STUB glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum,
                         const GLvoid *) {}
PSI_GL_STUB glTexImage2DMultisample(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLboolean) {}

// Framebuffers and renderbuffers.
inline void glGenFramebuffers(GLsizei n, GLuint *ids) { psi_glcompat::fill_names(n, ids); }
inline void glGenRenderbuffers(GLsizei n, GLuint *ids) { psi_glcompat::fill_names(n, ids); }
PSI_GL_STUB glDeleteFramebuffers(GLsizei, const GLuint *) {}
PSI_GL_STUB glBindFramebuffer(GLenum, GLuint) {}
PSI_GL_STUB glBindRenderbuffer(GLenum, GLuint) {}
PSI_GL_STUB glRenderbufferStorage(GLenum, GLenum, GLsizei, GLsizei) {}
PSI_GL_STUB glFramebufferRenderbuffer(GLenum, GLenum, GLenum, GLuint) {}
PSI_GL_STUB glFramebufferTexture(GLenum, GLenum, GLuint, GLint) {}
inline GLenum glCheckFramebufferStatus(GLenum) { return GL_FRAMEBUFFER_COMPLETE; }

#undef PSI_GL_STUB
