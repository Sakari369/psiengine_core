// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Main renderer class that handles rendering a RenderScene containing RenderObjs.

#pragma once

#include "PSIGlobals.h"
#include "PSIOpenGL.h"
#include "PSIMath.h"
#include "PSIRenderScene.h"
#include "PSIRenderObj.h"
#include "PSIGLTexture.h"
#include "PSIVideo.h"
#include "PSICamera.h"

class PSIGLRenderer {
	public:
		PSIGLRenderer() = default;
		~PSIGLRenderer() = default;

		// Draw mode. Do we render all objects with shader or wireframe ?
		enum DrawMode {
			SHADED = 0,
			WIREFRAME,
			WIREFRAME_BLENDED,
			drawMode_MAX = WIREFRAME_BLENDED
		};

		enum CullMode {
			DISABLED = 0,
			FRONT = 1,
			BACK = 2,
		};

		GLint init();
		void shutdown();

		void render(const shared_ptr<PSIRenderScene> &scene,
			       const shared_ptr<PSIRenderContext> &ctx, 
			       const shared_ptr<PSICamera> &camera);

		void draw_render_objs(const shared_ptr<PSIRenderScene> &scene,
		                      const shared_ptr<PSIRenderContext> &ctx,
		                      const shared_ptr<PSICamera> &camera);

		GLint cycle_draw_mode();
		GLint set_draw_mode(GLint draw_mode);

		void set_sorting(GLboolean sorting) {
			_sorting = sorting;
		}
		void set_msaa_samples(GLint msaa_samples) {
			_msaa_samples = msaa_samples;
		}
		void set_cull_mode(GLint cull_mode) {
			_cull_mode = cull_mode;
		}
		void set_wireframe(GLboolean wireframe) {
			_wireframe = wireframe;
		}
		shared_ptr<PSIRenderContext> get_context() {
			return _ctx;
		}

	private:
		// Current drawing context. Contains all the context variables that we need to pass around
		// while rendering.
		shared_ptr<PSIRenderContext> _ctx;

		GLint _draw_mode = DrawMode::SHADED;
		GLint _cull_mode = CullMode::BACK;

		// Options.
		bool _blending_enabled = true;
		// Render object as wireframe ?
		bool _wireframe = false;
		// Depth sort render objects ?
		bool _sorting = true;

		// Current MSAA level.
		GLfloat _msaa_samples = PSIVideo::DEF_MSAA_SAMPLES;
};
