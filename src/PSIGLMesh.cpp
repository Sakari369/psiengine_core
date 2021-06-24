#include "PSIGLMesh.h"

PSIGLMesh::PSIGLMesh(GLuint vao, GLsizei draw_count, GLuint draw_mode) :
		        _vao(vao), _draw_count(draw_count), _draw_mode(draw_mode) {
}

PSIGLMesh::~PSIGLMesh() {
	glDeleteVertexArrays(1, &_vao);
	glDeleteBuffers(BufferName::BufferName_MAX + 1, _buffer_name_ids);
}

void PSIGLMesh::gen_vao() {
	glGenVertexArrays(1, &_vao);
	check_gl_error();
};

void PSIGLMesh::gen_buffers() {
	// These are not yet bound to anything, so nothing is actually allocated
	// on the GL side yet, we only get placeholders for the ids.
	glGenBuffers(BufferName::BufferName_MAX + 1, _buffer_name_ids);
	check_gl_error();
};

bool PSIGLMesh::init() {
	gen_vao();
	gen_buffers();
	return true;
};

void PSIGLMesh::bind_vao() {
	glBindVertexArray(_vao);
	psilog(PSILog::OPENGL, "_vao = %d", _vao);
}

void PSIGLMesh::bind_buffer(GLenum target, GLuint buffer_name_id) {
	GLuint buffer_id = _buffer_name_ids[buffer_name_id];
	//psilog(PSILog::OPENGL, "target = %d buffer_name_id = %d buffer_id = %d", target, buffer_name_id, buffer_id);
	glBindBuffer(target, buffer_id);
}

void PSIGLMesh::enable_vertex_attrib(GLuint location, GLint size, GLsizei stride,
                                     const GLvoid *pointer, GLenum type) {
	GLboolean normalized = GL_FALSE;
	glVertexAttribPointer(location, size, type, normalized, stride, pointer);
	glEnableVertexAttribArray(location);
}

GLuint PSIGLMesh::enable_vertex_attrib(GLuint program, const GLchar *name,
                                       GLint size, GLsizei stride,
                                       const GLvoid *pointer, GLenum type) {
	GLboolean normalized = GL_FALSE;

	// Enable this vertex attribute
	GLuint location = glGetAttribLocation(program, name);
	if (location == GLMeshDefs::INVALID_ATTRIB_LOCATION) {
		psilog(PSILog::OPENGL, "Warning: Shader attribute with name '%s' not found in program = %d", name, program);

		return GLMeshDefs::INVALID_ATTRIB_LOCATION;
	}

	glVertexAttribPointer(location, size, type, normalized, stride, pointer);
	glEnableVertexAttribArray(location);

	return location;
}

void PSIGLMesh::draw() {
	glBindVertexArray(_vao);
	glDrawArrays(_draw_mode, 0, _draw_count);
}

void PSIGLMesh::draw_indexed() {
	/*
	plog("vao = %d _buffer_name_ids[BufferName::INDEX] = %d _draw_mode = %d _draw_count = %d", 
		_vao, _buffer_name_ids[BufferName::INDEX], _draw_mode, _draw_count);
	*/
	// We do not need to bind the buffers, as the buffer has already been bound in the vertex state
	// when we enable vertexAttribPointer.
	glBindVertexArray(_vao);
	glDrawElements(_draw_mode, _draw_count, _index_type, (void *)0);
}

void PSIGLMesh::draw_indexed(GLuint offset, GLuint count) {
	//plog("binding vertex array object = %d _buffer_name_ids[BufferName::INDEX] = %d", _vao, _buffer_name_ids[BufferName::INDEX]);
	glBindVertexArray(_vao);
	glDrawElements(_draw_mode, count, _index_type, reinterpret_cast<void*>(offset * sizeof(GLuint)));
}
