#include "PSIGLShader.h"

inline GLuint PSIGLShader::add_uniform(std::string name) {
	GLuint location = glGetUniformLocation(_program, (const GLchar *)name.c_str());
	_uniforms[name] = location;

	if (location == -1) {
		// This might happen also because the shader is optimizing a value not used from the shader.
		psilog(PSILog::OPENGL, "Program %d: Uniform location \"%s\" not found!", 
		     _program, (const GLchar *)name.c_str());
	} else {
		psilog(PSILog::OPENGL, "Program %d: Uniform added at location %d name=\"%s\"", _program, 
		     location, (const GLchar *)name.c_str());
	}

	check_gl_error();

	return location;
}

bool PSIGLShader::add_from_file(ShaderType type, std::string shader_path) {
	std::string shader_str = PSIFileUtils::read_file_to_string((const GLchar *) shader_path.c_str());
	if (shader_str.empty()) {
		psilog(PSILog::OPENGL, "Failed reading shader from file \"%s\"", shader_path.c_str());
		return false;
	} else {
		psilog(PSILog::OPENGL, "Loaded shader from file \"%s\"", shader_path.c_str());
	}

	if (add_from_string(type, shader_str) == false) {
		psilog(PSILog::OPENGL, "Failed loading shader from path \"%s\"", shader_path.c_str());
		return false;
	}

	return true;
}

bool PSIGLShader::add_from_string(ShaderType type, std::string shader_str) {
	assert(!shader_str.empty());

	std::tuple<GLenum, std::string> info = get_shader_type_info(type);
	GLuint shader_obj = create_shader_obj(std::get<0>(info), shader_str);

	if (shader_obj == ShaderDefs::INVALID_SHADER) {
		psilog(PSILog::OPENGL, "Failed creating %s shader", std::get<1>(info).c_str());
		return false;
	} else if (shader_obj == ShaderDefs::COMPILE_FAILED) {
		psilog(PSILog::OPENGL, "Failed compiling %s shader", std::get<1>(info).c_str());
		return false;
	}

	// Attach to our program.
	assert(_program != ShaderDefs::INVALID_SHADER);
	glAttachShader(_program, shader_obj);

	// Keep reference of the shaders we have added.
	_shader_objs.push_back(shader_obj);

	psilog(PSILog::OPENGL, "Added %s shader to program '%s'", std::get<1>(info).c_str(), get_name().c_str());

	return true;
}

void PSIGLShader::add_transform_feedback_varyings(std::vector<std::string> varyings, GLboolean interleaved) {
	GLenum mode;
	if (interleaved) {
		mode = GL_INTERLEAVED_ATTRIBS;
	} else {
		mode = GL_SEPARATE_ATTRIBS;
	}

	GLulong count = varyings.size();
	GLchar const *strings[count];

	// Convert to C-strings.
	for (GLulong i = 0; i<count; i++) {
		strings[i] = varyings.at(i).c_str();
		psilog(PSILog::OPENGL, "Added '%s'", strings[i]);
	}

	glTransformFeedbackVaryings(_program, count, strings, mode);
	check_gl_error();
}

GLuint PSIGLShader::add_uniforms() {
	GLint num_uniforms = 0;
	GLint arr_size = 0;
	GLenum type = 0;
	GLsizei str_len = 0;
	GLint max_str_len = 0;

	assert(_program != ShaderDefs::INVALID_SHADER);

	// Get number of attributes and uniforms.
	glGetProgramiv(_program, GL_ACTIVE_UNIFORMS, &num_uniforms);

	//psilog(PSILog::MSG, "num_attribs = %d num_uniforms = %d", num_attribs, num_uniforms);
	// Get attributes
	/*
	glGetProgramiv(__program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &max_str_len);
	std::vector<GLchar> attrib_names(max_str_len);
	for(int i = 0; i < num_attribs; i++) {
		glGetActiveAttrib(__program, i, attrib_names.size(), &str_len, &arr_size, &type, &attrib_names[0]);
		std::string name((char*)&attrib_names[0], str_len);
		//psilog(PSILog::MSG, "arr_size = %d type = %d str_len = %d name = %s", arr_size, type, str_len, name.c_str());
	}
	*/

	// Get uniforms.
	glGetProgramiv(_program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_str_len);
	std::vector<GLchar> uniform_names(max_str_len);

	for(int i = 0; i < num_uniforms; i++) {
		glGetActiveUniform(_program, i, uniform_names.size(), &str_len, &arr_size, &type, &uniform_names[0]);
		std::string name((char*)&uniform_names[0], str_len);

		// Add our uniforms automatically.
		add_uniform(name);

		//psilog(PSILog::MSG, "arr_size = %d type = %d str_len = %d name = %s", arr_size, type, str_len, name.c_str());
	}

	return num_uniforms;
}

GLuint PSIGLShader::create_program() {
	GLuint program = glCreateProgram();
	if (program == 0) {
		PSI_G::log(PSILog::FAIL) << "Failed creating shader program!" << std::endl;
		return ShaderDefs::INVALID_SHADER;
	}

	// Store as our current program.
	_program = program;

	return _program;
}

GLuint PSIGLShader::compile() {
	GLuint link_status = link_program();

	if (link_status == ShaderDefs::INVALID_SHADER) {
		PSI_G::log(PSILog::FAIL) << "Failed compiling shader '"<< _name << "'!" << std::endl;

		return ShaderDefs::INVALID_SHADER;
	} else if (link_status == ShaderDefs::LINK_FAILED) {
		PSI_G::log(PSILog::FAIL) << "Failed linking shader '"<< _name << "'!" << std::endl;

		return ShaderDefs::INVALID_SHADER;
	}

	// Detach after compile
	for(GLuint &shaderObj : _shader_objs) {
		glDetachShader(_program, shaderObj);
		glDeleteShader(shaderObj);
	}

	_shader_objs.clear();

	PSI_G::log(PSILog::OPENGL) << "Shader '" << _name << "'"
				   << "compiled with id = " << _program << std::endl;

	return _program;
}

GLuint PSIGLShader::link_program() {
	GLint info_len;
	GLint status;
	GLchar *info_log;

	assert(_program != ShaderDefs::INVALID_SHADER);

	// Link and check status
	glLinkProgram(_program);

	glGetProgramiv(_program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) {
		glGetProgramiv(_program, GL_INFO_LOG_LENGTH, &info_len);
		info_log = new GLchar[info_len + 1];

		glGetProgramInfoLog(_program, info_len, NULL, info_log);
		fprintf(stderr, "%s", info_log);

		delete[] info_log;

		return ShaderDefs::LINK_FAILED;
	}

	return _program;
}

std::tuple<GLenum, std::string> PSIGLShader::get_shader_type_info(ShaderType type) {
	std::string name;
	GLenum gl_type;

	switch(type) {
	case ShaderType::VERTEX: 
		name = "vertex"; 
		gl_type = GL_VERTEX_SHADER;
		break;

	case ShaderType::GEOMETRY: 
		name = "geometry"; 
		gl_type = GL_GEOMETRY_SHADER;
		break;

	case ShaderType::FRAGMENT:
		name = "fragment"; 
		gl_type = GL_FRAGMENT_SHADER;
		break;
	}

	return std::make_tuple(gl_type, name);
}

GLuint PSIGLShader::create_shader_obj(GLenum type, const std::string &shader_str) {
	// Create shader.
	GLuint shader = glCreateShader(type);
	if (shader == 0) {
		fprintf(stderr, "Failed creating shader\n");
		return ShaderDefs::INVALID_SHADER;
	}

	const char *shader_buf = shader_str.c_str();
	glShaderSource(shader, 1, &shader_buf, NULL);

	// Compile.
	glCompileShader(shader);

	// Check for errors.
	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		GLint info_len;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);

		GLchar *info_log = new GLchar[info_len + 1];
		glGetShaderInfoLog(shader, info_len, NULL, info_log);

		fprintf(stderr, "Shader program compile failure:\n%s\n", info_log);
		delete[] info_log;

		return ShaderDefs::COMPILE_FAILED;
	}

	return shader;
}
