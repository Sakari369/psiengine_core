// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// OpenGL shader interface.

#pragma once

#include <iostream>
#include <math.h>
#include <unordered_map>

#include "PSIGlobals.h"
#include "PSIOpenGL.h"
#include "PSIFileUtils.h"
#include "PSIGLUtils.h"

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
		~PSIGLShader() = default;

		// Setting shader uniforms.
		template <typename Type>
		void set_uniform(const std::string &name, Type &&value) {
			GLuint location = _uniforms[name];
			set_uniform(location, std::forward<Type>(value));
			/*
			 * DEBUG VERSION BELOW
			 */
			/*
			if (_uniforms.count(name) > 0) {
				set_uniform(location, std::forward<Type>(value));
				if (check_gl_error()) {
					plog_s("Error setting uniform \"%s\" !", (const GLchar *)name.c_str());
				}
			} else {
				plog_s("Program %d: Uniform \"%s\" not found!", _program, (const GLchar *)name.c_str());
			}
			*/
		}

		// Setting static vertex attribute.
		template <typename Type>
		void vertex_attrib(GLuint index, Type &&value) {
			//plog_s("Setting vertex_attrib, index = %d", index);
			set_vertex_attrib(index, std::forward<Type>(value));
		}

		// Use this shader program.
		void use_program() {
			glUseProgram(_program);
		}

		// Return uniform location in shader for uniform name.
		GLuint get_uniform(std::string name) {
			assert(_uniforms.count(name) > 0);
			return _uniforms[name];
		}

		// Setting different type uniform values.
		void set_uniform(GLuint location, const GLint &val) {
			glUniform1i(location, val);
		}
		void set_uniform(GLuint location, const GLuint &val) {
			glUniform1ui(location, val);
		}
		void set_uniform(GLuint location, const GLfloat &val) {
			glUniform1f(location, val);
		}
		void set_uniform(GLuint location, const glm::vec2 &vec) {
			glUniform2f(location, vec.x, vec.y);
		}
		void set_uniform(GLuint location, const glm::vec3 &vec) {
			glUniform3f(location, vec.x, vec.y, vec.z);
		}
		void set_uniform(GLuint location, const glm::vec4 &vec) {
			glUniform4f(location, vec.x, vec.y, vec.z, vec.w);
		}
		void set_uniform(GLuint location, const glm::mat3 &mat) {
			glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(mat[0]));
		}
		void set_uniform(GLuint location, const glm::mat4 &mat) {
			glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(mat[0]));
		}

		// Vertex attribute setting.
		void set_vertex_attrib(GLuint index, const glm::vec2 &vec) {
			glVertexAttrib2f(index, vec.x, vec.y);
		}
		void set_vertex_attrib(GLuint index, const glm::vec3 &vec) {
			glVertexAttrib3f(index, vec.x, vec.y, vec.z);
		}
		void set_vertex_attrib(GLuint index, const glm::vec4 &vec) {
			glVertexAttrib4f(index, vec.x, vec.y, vec.z, vec.w);
		}

		// Add uniform variable for this shader.
		inline GLuint add_uniform(std::string name);
		// Query active uniforms for the current program.
		GLuint add_uniforms();
		// Specify variable names to record in transform feedback buffers.
		void add_transform_feedback_varyings(std::vector<std::string> varyings, GLboolean interleaved);

		// Add shader from file to this shader collection.
		bool add_from_file(ShaderType type, std::string shader_path);
		// Add shader from string to this shader collection.
		bool add_from_string(ShaderType type, std::string shader_str);

		// Compiles the shader program from our read shaders.
		GLuint compile();
		// Create and return shader program id.
		GLuint create_program();

		// Get current shader program id.
		GLuint get_program() {
			return _program;
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

	private:
		// Our currently compiled shader program.
		GLuint _program = ShaderDefs::INVALID_SHADER;
		// Name for this shader.
		std::string _name;
		// The shaders we have attached before compilation stage.
		std::vector<GLuint> _shader_objs;
		// Our uniform locations in the shader, mapped by name.
		std::unordered_map <std::string, GLuint> _uniforms;
		// Get shader type and shader name from shader type.
		std::tuple<GLenum, std::string> get_shader_type_info(ShaderType type);
		// Create shader with type from shader string.
		GLuint create_shader_obj(GLenum type, const std::string &shader_str);
		// Link shader program.
		GLuint link_program();
};
