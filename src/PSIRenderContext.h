// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Defines a rendering context. For passing around current rendering settings.

#pragma once

#include "PSIGlobals.h"
#include "PSIOpenGL.h"
#include "PSICamera.h"
#include "PSILight.h"

class PSIRenderContext {
	private:
	public:
		PSIRenderContext() = default;
		~PSIRenderContext() = default;

		// Elapsed time since the beginning of this context 
		GLfloat elapsed_time = 0.0f;
		// Elapsed frames since the beginning of this context
		GLuint elapsed_frames = 0;

		// Time of last frame in ms
		GLfloat frametime = 0.0f;
		// FPS dependent frame multiplier
		GLfloat frametime_mult = (1.0f / 60.0f) * 1000.0f;

		// The current interpolation step between this and the previous physics state
		GLfloat transform_interpolation = 0.0f;

		// Current global rendering opacity.
		GLfloat opacity = 1.0f;

		// force wireframe drawing ?
		GLboolean wireframe = false;
		// Framebuffer objects we use
		GLuint main_fbo = 0;
		GLuint msaa_fbo = 0;

		// background color 
		glm::vec4 bg_color = glm::vec4(0.2f, 0.2f, 0.2, 1.0f);
		// viewport size
		glm::vec2 viewport_size = glm::vec2(0.0f, 0.0f);

		// model view projection stacks
		// for easy restoration of mvp matrixes
		std::stack<glm::mat4> model;
		std::stack<glm::mat4> view;
		std::stack<glm::mat4> projection;

		// The camera view into the world
		shared_ptr<PSICamera> camera_view;
		// Ambient light
		std::vector<shared_ptr<PSILight>> lights;
};
