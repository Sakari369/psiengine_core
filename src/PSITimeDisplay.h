// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Renders time in HH:MM:SS formatting.
// Optimized for this usecase.

#pragma once

#include <stdio.h>
#include <wchar.h>

#include "PSIGlobals.h"
#include "PSIMath.h"
#include "PSITextRenderer.h"

class PSITimeDisplay {
	public:
		PSITimeDisplay() = default;
		~PSITimeDisplay() = default;

		void set_shader(ShaderSharedPtr shader);
		void set_atlas(texture_atlas_t *atlas);

		GLboolean init();

		void logic(GLfloat frameTimeMs);
		void draw(const RenderContextSharedPtr &ctx);

	private:
		static const std::string NUMBER_STR;
		static const GLfloat OPERATING_FREQ;
		static const GLint SEPARATOR_IDX;
		static const GLint FRACTION_IDX;
		static const GLint PAIR_COUNT = 3;

		GLfloat _remainder = 0.0f;

		// Our display numbers
		// Minutes, Seconds, Milliseconds
		GLint _numbers[PAIR_COUNT][2] = { {0, 0}, {0, 0}, {0, 0} };

		std::string _font_path = "assets/fonts/VeraMono.ttf";
		GLint _font_size = 32;
		glm::vec4 _font_color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		glm::vec2 _text_pos = glm::vec2(24.0f, 48.0f);

		PSITextRenderer _renderer;
};
