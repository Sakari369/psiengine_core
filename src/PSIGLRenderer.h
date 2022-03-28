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

class PSIGLRenderer;
typedef shared_ptr<PSIGLRenderer> GLRendererSharedPtr;

class PSIGLRenderer {
	public:
	 	// The renderer needs the viewport size.
		PSIGLRenderer(glm::ivec2 viewport_size) {
			_viewport_size = viewport_size;
		}
		~PSIGLRenderer() = default;

		// Draw mode. Do we render all objects with shader or wireframe ?
		enum DrawMode {
			SHADED = 0,
			WIREFRAME,
			WIREFRAME_BLENDED,
			DrawMode_MAX = WIREFRAME_BLENDED
		};

		enum CullMode {
			DISABLED = 0,
			FRONT = 1,
			BACK = 2,
		};
		
		// Static creation method.
		static GLRendererSharedPtr create(glm::ivec2 viewport_size) {
			return make_shared<PSIGLRenderer>(viewport_size);
		}

		GLint init();
		void shutdown();

		void render(const RenderSceneSharedPtr &scene,
			       const RenderContextSharedPtr &ctx, 
			       const CameraSharedPtr &camera);

		void draw_render_objs(const RenderSceneSharedPtr &scene,
		                      const RenderContextSharedPtr &ctx,
		                      const CameraSharedPtr &camera);

		// Setup shader uniforms for lights.
		void setup_lights(const ShaderSharedPtr &shader, const RenderContextSharedPtr &ctx);

		// Initialize texture where we should render, if rendering scene to texture.
		GLint init_offscreen_texture(glm::ivec2 size);

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

		RenderContextSharedPtr get_context() {
			return _ctx;
		}

		void set_viewport_size(glm::ivec2 size) {
			_viewport_size = size;
		}

		GLTextureSharedPtr get_offscreen_texture() {
			return _offscreen_texture;
		}

		GLuint get_offscreen_fbo() {
			return _offscreen_fbo;
		}

		GLuint get_offscreen_depth_buffer() {
			return _offscreen_depth_buffer;
		}

		// Store reference to the PSIVideo instance.
		void set_video(const shared_ptr<PSIVideo> &video) {
			_video = video;
		}

		// Write current OpenGL buffer to PNG file.
		bool write_screen_to_file(std::string path);

	private:
		// Current drawing context. Contains all the context variables that we need to pass around while rendering.
		RenderContextSharedPtr _ctx;

		// The video instance reference for accessing the video data and so on.
		shared_ptr<PSIVideo> _video;

		// Offscreen framebuffer we are rendering to.
		GLuint _offscreen_fbo = -1;

		// Offscreen texture we are rendering to.
		GLTextureSharedPtr _offscreen_texture;

		// Offscreen depth buffer.
		GLuint _offscreen_depth_buffer = -1;

		// Draw mode sets wireframe and blending together.
		GLint _draw_mode = DrawMode::SHADED;
		// What face side are we culling, or none.
		GLint _cull_mode = CullMode::BACK;

		// Enable GL_BLEND ?
		bool _blending_enabled = true;
		// Render object as wireframe ?
		bool _wireframe = false;
		// Depth sort render objects ?
		bool _sorting = true;
		// Current MSAA level.
		GLfloat _msaa_samples = PSIVideo::DEF_MSAA_SAMPLES;
		// Viewport size.
		glm::ivec2 _viewport_size = glm::ivec2(0, 0);
};
