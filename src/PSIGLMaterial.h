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
		// Does this material need alpha blending?
		//
		// AUTO derives it; see wants_blending(). ON and OFF are what
		// set_blending() writes, for the cases the derivation cannot know about.
		enum BlendMode {
			BLEND_AUTO = -1,
			BLEND_OFF  = 0,
			BLEND_ON   = 1,
		};

		PSIGLMaterial() = default;
		~PSIGLMaterial() = default;

		// Copy constructor.
		PSIGLMaterial(const PSIGLMaterial &rhs) :
				_color(rhs._color),
				_wireframe(rhs._wireframe),
				_lit(rhs._lit),
				_textured(rhs._textured),
				_blend_mode(rhs._blend_mode),
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

		// Draw-path accessors.
		//
		// The by-value getters above are what Lua binds and they stay, but every
		// one of them is an atomic increment and decrement on the shared_ptr
		// control block. The draw path touches the shader, the texture and the
		// mesh on every object of every frame, and the object owns them for the
		// whole call, so it takes references instead.
		const ShaderSharedPtr &shader_ref() const {
			return _shader;
		}
		const GLTextureSharedPtr &texture_ref() const {
			return _texture;
		}

		// Should we have a clearTexture 
		void set_texture(GLTextureSharedPtr texture) {
			_texture = texture;
			_textured = true;
		}
		GLTextureSharedPtr get_texture() {
			return _texture;
		}
		bool has_texture() const {
			return _texture != nullptr;
		}

		void set_textured(GLboolean textured) {
			_textured = textured;
		}
		GLboolean get_textured() {
			return _textured;
		}

		// Blending.
		//
		// The GL renderer kept GL_BLEND on for the entire frame, so the Metal
		// port baked SRC_ALPHA / ONE_MINUS_SRC_ALPHA into every pipeline. On a
		// tile-based deferred GPU that is expensive in a way it was not on a
		// desktop GL driver: a blended fragment depends on what is already in
		// the tile, so hidden surface removal cannot discard anything, and every
		// overlapped fragment shades.
		//
		// The safety argument is exact rather than approximate. With
		// SRC_ALPHA / ONE_MINUS_SRC_ALPHA and a source alpha of 1.0 the equation
		// is 1*src + 0*dst -- the destination contributes nothing, so turning
		// blending off cannot change the pixel.
		//
		// AUTO therefore says opaque only when the material's alpha is 1 and it
		// carries no texture, because a texture's own alpha reaches the blend
		// stage without passing through this colour (phong_textured_alpha is
		// exactly that shader). Anything else stays blended.
		void set_blending(GLboolean enabled) {
			_blend_mode = enabled ? BLEND_ON : BLEND_OFF;
		}
		void set_blending_auto() {
			_blend_mode = BLEND_AUTO;
		}
		GLboolean get_blending() {
			return wants_blending();
		}
		bool wants_blending() const {
			if (_blend_mode != BLEND_AUTO) {
				return _blend_mode == BLEND_ON;
			}
			return !(_color.a >= 1.0f && _textured == false);
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
		// Blending override; see set_blending().
		GLint _blend_mode = BLEND_AUTO;
		// Texture for this material.
		GLTextureSharedPtr _texture;
		// Shader that is used to render this material.
		ShaderSharedPtr _shader;
};
