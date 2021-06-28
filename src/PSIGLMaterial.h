// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// A class defining a simple OpenGL material type.

#pragma once

#include <iostream>
#include <math.h>

#include "PSIGlobals.h"
#include "PSIOpenGL.h"
#include "PSIGLShader.h"
#include "PSIGLTexture.h"

class PSIGLMaterial;
typedef shared_ptr<PSIGLMaterial> GLMaterialSharedPtr;

class PSIGLMaterial {
	public:
		PSIGLMaterial() = default;
		~PSIGLMaterial() = default;

		// Copy constructor.
		PSIGLMaterial(const PSIGLMaterial &rhs) : 
				_color(rhs._color),
				_wireframe(rhs._wireframe),
				_lit(rhs._lit),
				_textured(rhs._textured),
				_texture(rhs._texture),
				_shader(rhs._shader) {
		}

		static GLMaterialSharedPtr create() {
			return make_shared<PSIGLMaterial>();
		}

		GLMaterialSharedPtr clone() {
			return make_shared<PSIGLMaterial>(*this);
		}

		void set_color(glm::vec4 color) {
			if (_color != color) {
				_color = color;
				set_needs_update(true);
			}
		}
		glm::vec4 get_color() {
			return _color;
		}

		void set_opacity(GLfloat opacity) {
			_color.a = opacity;
		}
		GLfloat get_opacity() {
			return _color.a;
		}

		void set_wireframe(GLboolean wireframe) {
			_wireframe = wireframe;
		}
		GLboolean get_wireframe() {
			return _wireframe;
		}

		void set_lit(GLboolean lit) {
			_lit = lit;
		}
		GLboolean is_lit() {
			return _lit;
		}

		void set_shader(ShaderSharedPtr shader) {
			_shader = shader;
		}
		ShaderSharedPtr get_shader() {
			return _shader;
		}

		// Should we have a clearTexture 
		void set_texture(GLTextureSharedPtr texture) {
			_texture = texture;
			_textured = true;
		}
		GLTextureSharedPtr get_texture() {
			return _texture;
		}

		void set_textured(GLboolean textured) {
			_textured = textured;
		}
		GLboolean get_textured() {
			return _textured;
		}

		void set_needs_update(GLboolean needs_update) {
			_needs_update = needs_update;
		}
		GLboolean needs_update() {
			return _needs_update;
		}

	private:
		// Basic material color.
		glm::vec4 _color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		// Render as wireframe ?
		GLboolean _wireframe = false;
		// Does lightning affect this material ?
		GLboolean _lit = true;
		// Is the material textured ?
		GLboolean _textured = false;
		// Do we need to update GPU data for render objects with this material ?
		GLboolean _needs_update = false;
		// Texture for this material.
		GLTextureSharedPtr _texture;
		// Shader that is used to render this material.
		ShaderSharedPtr _shader;
};
