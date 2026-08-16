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

		// Discard objects whose bounds fall entirely outside the camera frustum.
		//
		// Off by default and deliberately so. Scripts already control visibility
		// themselves through set_visible(), the bounds are a CPU-side
		// approximation, and it only pays on scenes with many separate objects --
		// prism_grid draws 9600 prisms in one instanced call, where culling can
		// do nothing at all. Turn it on for scenes like starfield that scatter
		// hundreds of individually-drawn objects well past the view.
		//
		// The renderer never culls objects whose bounds do not describe where
		// they are drawn: camera-locked ones (the skybox uses a different view
		// matrix), instanced meshes (the bounds cover the base mesh, not the
		// instance spread), objects with depth testing off, objects with
		// children, and anything without real geometry bounds. A vertex shader
		// that displaces geometry is the case only the script can know about --
		// see PSIRenderObj::set_cullable().
		void set_frustum_culling(GLboolean enabled) {
			_frustum_culling = enabled;
		}
		GLboolean get_frustum_culling() {
			return _frustum_culling;
		}

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

		// Framebuffer objects do not exist in Metal -- a render pass names its
		// attachments directly. These stay because they are bound to Lua
		// (LuaAPI.cpp:542-543), but there is no handle to hand back; use
		// get_offscreen_texture() instead. No shipped script calls either.
		GLuint get_offscreen_fbo() {
			return 0;
		}

		GLuint get_offscreen_depth_buffer() {
			return 0;
		}

		// Store reference to the PSIVideo instance.
		// Also picks up the Metal context, which PSIVideo owns -- render() and
		// PSIVideo::flip() both operate on the same frame through it.
		void set_video(const shared_ptr<PSIVideo> &video) {
			_video = video;
			if (video != nullptr) {
				_metal_ctx = video->get_metal_context();
			}
		}

		MetalContextSharedPtr get_metal_context() {
			return _metal_ctx;
		}

		// Write current OpenGL buffer to PNG file.
		bool write_screen_to_file(std::string path, int format);

		// Start copying every presented frame aside so it can be written out.
		//
		// write_screen_to_file() arms this itself, but only from the frame after
		// the first call -- see there. A script exporting from frame 0 calls this
		// during setup instead. Additive; nothing needs it.
		void set_frame_capture(GLboolean enabled) {
			if (enabled && _metal_ctx != nullptr) {
				_metal_ctx->arm_capture();
			}
		}

	private:
		// Current drawing context. Contains all the context variables that we need to pass around while rendering.
		RenderContextSharedPtr _ctx;

		// The video instance reference for accessing the video data and so on.
		shared_ptr<PSIVideo> _video;

		// Metal device, queue and swapchain. Owned by PSIVideo.
		MetalContextSharedPtr _metal_ctx;

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
		// Cull objects outside the camera frustum ? See set_frustum_culling().
		bool _frustum_culling = false;

		// The six frustum planes of the current pass, as (a, b, c, d) with the
		// inside on the positive side. Extracted once per render() from
		// projection * view.
		glm::vec4 _frustum_planes[6];
		void extract_frustum_planes(const glm::mat4 &view_projection);
		bool is_inside_frustum(PSIRenderObj *obj, const RenderContextSharedPtr &ctx) const;
		// Current MSAA level.
		GLfloat _msaa_samples = PSIVideo::DEF_MSAA_SAMPLES;
		// Viewport size.
		glm::ivec2 _viewport_size = glm::ivec2(0, 0);
};
