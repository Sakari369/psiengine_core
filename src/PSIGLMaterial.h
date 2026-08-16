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
				_textures(rhs._textures),
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

		// Bind every texture this material carries onto the active encoder.
		//
		// Each name is resolved against the shader's reflected binding indices,
		// so a texture lands in the slot that shader declared it at. A name the
		// shader does not declare resolves to -1 and is skipped, which is what
		// lets one material serve a shader reading three maps and another
		// reading one.
		void bind_textures(const ShaderSharedPtr &shader) const {
			if (shader == nullptr) {
				return;
			}

			for (const auto &entry : _textures) {
				const GLint slot = shader->get_texture_slot(entry.first);
				if (slot < 0 || entry.second == nullptr) {
					continue;
				}
				entry.second->bind(slot, shader->get_sampler_slot(entry.first));
			}
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
		const std::unordered_map<std::string, GLTextureSharedPtr> &textures_ref() const {
			return _textures;
		}

		// The name a texture takes when none is given.
		//
		// Every shader in the tree calls its one texture u_diffuse, so an
		// unnamed set_texture() means that one.
		static constexpr const char *DEFAULT_TEXTURE = "u_diffuse";

		// Should we have a clearTexture
		void set_texture(GLTextureSharedPtr texture) {
			set_texture_named(DEFAULT_TEXTURE, texture);
		}
		GLTextureSharedPtr get_texture() {
			return get_texture_named(DEFAULT_TEXTURE);
		}

		// Attach a texture under the name its shader declares it by.
		//
		// A material can carry as many as its shader reads -- albedo, normal,
		// roughness -- and each is bound at the slot reflection reported for
		// that name, rather than everything landing in slot 0 as it did when a
		// material held exactly one texture.
		//
		// Naming rather than numbering means one material works with shaders
		// that read different subsets: a name the shader does not declare is
		// simply not bound.
		void set_texture_named(std::string name, GLTextureSharedPtr texture) {
			if (texture == nullptr) {
				_textures.erase(name);
			} else {
				_textures[name] = texture;
			}
			_textured = !_textures.empty();
		}

		GLTextureSharedPtr get_texture_named(const std::string &name) {
			auto it = _textures.find(name);
			return (it == _textures.end()) ? nullptr : it->second;
		}

		bool has_texture() const {
			return !_textures.empty();
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
		// Textures by the name their shader declares them under; see
		// set_texture_named(). Replaces the single _texture slot.
		std::unordered_map<std::string, GLTextureSharedPtr> _textures;
		// Shader that is used to render this material.
		ShaderSharedPtr _shader;
};
