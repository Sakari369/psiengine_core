// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// PSIFontAtlas and PSITextRenderer.
// Text renderer, interfacing freetype-gl.

#pragma once

#include <stdio.h>
#include <wchar.h>

#include "PSIGlobals.h"
#include "PSIMath.h"
#include "PSIGeometry.h"
#include "PSIRenderObj.h"
#include "PSIString.h"

#include "ext/freetype-gl/freetype-gl.h"
#include "ext/freetype-gl/texture-font.h"

class PSIFontAtlas {
	public:
		PSIFontAtlas() = default;
		~PSIFontAtlas() {
			delete_font();
		}

		// Initialize font atlas from loaded font.
		GLboolean init();

		texture_font_t *get_font() {
			return _font;
		}

		texture_atlas_t *get_atlas() {
			return _atlas;
		}

		void delete_font() {
			texture_font_delete(_font);
		}

		void set_charset(std::string charset) {
			_charset = charset;
		}

		void set_font_path(std::string font_path) {
			_font_path = font_path;
		}
		std::string get_font_path() {
			return _font_path;
		}

		void set_font_size(GLint font_size) {
			_font_size = font_size;
		}
		GLint get_font_size() {
			return _font_size;
		}

		void set_atlas_texture_id(GLuint id) {
			_atlas->id = id;
		}

		void set_font_color(glm::vec4 font_color) {
			_font_color = font_color;
		}
		glm::vec4 get_font_color() {
			return _font_color;
		}

		void set_size(glm::vec2 size) {
			_size = size;
		}
		glm::vec2 get_size() {
			return _size;
		}

	private:
		// Pointer to texture atlas.
		texture_atlas_t *_atlas;
		// Total size of the font atlas.
		glm::vec2 _size;
		// Font for the texture atlas.
		texture_font_t *_font;
		// Font size.
		GLint _font_size;
		// Font color.
		glm::vec4 _font_color;
		// Path to font to be loaded into the atlas.
		std::string _font_path;
		// Character set that this atlas contains in it's texture.
		std::string _charset;

		// Create texture atlas.
		texture_atlas_t *create_atlas(GLsizei width, GLsizei height);
};

class PSITextRenderer : public PSIRenderObj {
	using shared_data = shared_ptr<PSIGeometryData>;
	using unique_data = unique_ptr<PSIGeometryData>;

	public:
		PSITextRenderer() = default;
		~PSITextRenderer() = default;

		GLboolean init();

		void draw(const shared_ptr<PSIRenderContext> &ctx);

		void set_text(std::string text);
		std::string get_text() {
			return _text;
		}

		void set_draw_offset(GLuint draw_offset) {
			_draw_offset = draw_offset;
		}

		void set_draw_count(GLuint draw_count) {
			_draw_count = draw_count;
		}


		glm::vec2 get_dimensions() {
			return _dimensions;
		}

		glm::vec2 get_glyph_dimensions(GLint idx) {
			return _glyph_dimensions.at(idx);
		}

		shared_ptr<PSIFontAtlas> get_font_atlas() {
			return _font_atlas;
		}

		void set_font_atlas(shared_ptr<PSIFontAtlas> font_atlas) {
			_font_atlas = font_atlas;
		}

	private:
		// The font data this text is using.
		shared_ptr<PSIFontAtlas> _font_atlas;

		// Dimensions of the currently baked text.
		glm::vec2 _dimensions = glm::vec2(0.0f, 0.0f);
		// Individual glyph dimensions for currently baked text.
		std::vector<glm::vec2> _glyph_dimensions;
		// Our text string in wide UTF format.
		std::string _text = "";

		// Are we offseting glyphs while drawing ?
		// Used for certain effects.
		GLboolean _offset_glyphs = true;
		// Offset for drawing the text, from which char to begin.
		GLuint _draw_offset = -1;
		// How many characters to draw from that offset.
		GLuint _draw_count = -1;

		// Bake current text into a mesh.
		unique_data bake_text(const shared_ptr<PSIFontAtlas> &atlas, std::string text);
		// Update current text mesh.
		void update_mesh();
};
