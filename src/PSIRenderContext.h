// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Defines a rendering context. For passing around current rendering settings.

#pragma once

#include "PSIGlobals.h"
#include "PSIOpenGL.h"
#include "PSICamera.h"
#include "PSILight.h"
#include "PSIGLShader.h"

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

		// Background color.
		glm::vec4 bg_color = glm::vec4(0.2f, 0.2f, 0.2, 1.0f);

		// Model-view-projection stacks.
		// For easy restoration of mvp matrixes.
		std::stack<glm::mat4> model;
		std::stack<glm::mat4> view;
		std::stack<glm::mat4> projection;

		// Current camera the scene is being rendered with.
		CameraSharedPtr camera;
		// Scene lights.
		std::vector<LightSharedPtr> lights;

		// Draw every object with this shader instead of its own material's.
		//
		// Null for all normal drawing, which is every path that existed before
		// temporal antialiasing. The velocity pass sets it so one position-only
		// shader stands in for the whole scene's worth of materials -- that is
		// what keeps velocity.metal from having to be duplicated into each of
		// the eighteen shaders in assets/shaders, and lets a script's inline
		// shader take part without knowing TAA exists.
		//
		// Two of them, because an instanced mesh needs the vertex stage that
		// reads PSIInstanceData and PSIGLShader picks its entry point by name at
		// compile time, so one shader object cannot serve both.
		ShaderSharedPtr shader_override;
		ShaderSharedPtr shader_override_instanced;

		// True while the velocity pass is the one encoding. Objects use it to
		// decide whether to roll their previous-frame matrices over; see
		// PSIRenderObj::draw().
		bool velocity_pass = false;

		// This frame's sub-pixel offset, in NDC, already folded into
		// `projection`. Zero unless TAA is on.
		//
		// Passed to shaders as u_jitter, for the ones that rebuild a ray from
		// the projection instead of taking clip space as given.
		glm::vec2 jitter_ndc = glm::vec2(0.0f, 0.0f);
};
