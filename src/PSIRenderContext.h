// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Defines a rendering context. For passing around current rendering settings.

#pragma once

#include "PSIGlobals.h"
#include "PSIOpenGL.h"
#include "PSICamera.h"
#include "PSILight.h"

class PSIRenderContext;
typedef shared_ptr<PSIRenderContext> RenderContextSharedPtr;

class PSIRenderContext {
	private:
	public:
		PSIRenderContext() = default;
		~PSIRenderContext() = default;

		static RenderContextSharedPtr create() {
			return make_shared<PSIRenderContext>();
		}

		// Elapsed time since the beginning of this context.
		GLfloat elapsed_time = 0.0f;
		// Elapsed frames since the beginning of this context.
		GLuint elapsed_frames = 0;

		// Time of last frame in ms.
		GLfloat frametime = 0.0f;
		// FPS dependent frame multiplier.
		GLfloat frametime_mult = (1.0f / 60.0f) * 1000.0f;

		// The current interpolation step between this and the previous physics state.
		GLfloat transform_interpolation = 0.0f;

		// Current global rendering opacity.
		GLfloat opacity = 1.0f;

		// Force wireframe drawing ?
		GLboolean wireframe = false;
		// OpenGL Framebuffer objects we are using.
		GLuint main_fbo = 0;
		GLuint msaa_fbo = 0;

		// Background color.
		glm::vec4 bg_color = glm::vec4(0.2f, 0.2f, 0.2, 1.0f);
		// Viewport size.
		glm::vec2 viewport_size = glm::vec2(0.0f, 0.0f);

		// Model-view-projection stacks.
		// For easy restoration of mvp matrixes.
		std::stack<glm::mat4> model;
		std::stack<glm::mat4> view;
		std::stack<glm::mat4> projection;

		// Current camera the scene is being rendered with.
		CameraSharedPtr camera;
		// Scene lights.
		std::vector<LightSharedPtr> lights;
};
