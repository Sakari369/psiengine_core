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
#include "PSIRenderPass.h"
#include "PSIRenderTarget.h"
#include "PSIFrame.h"

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

		// Open a frame and get the handle passes are encoded into.
		//
		// The handle is created once and reused, so this allocates nothing per
		// frame. See PSIFrame for the shape this replaces.
		const FrameSharedPtr &begin_frame();

		// Present and close the frame. Called by PSIFrame::present().
		//
		// Goes through here rather than straight to the context so the video
		// layer still sees the frame -- the benchmark and capture harnesses
		// count frames in PSIVideo, and a script on the pass API would
		// otherwise lose both.
		void end_frame();

		// Encode one pass. Called by PSIFrame; scripts go through the frame.
		void encode_pass(const RenderPassSharedPtr &pass,
		                 const RenderSceneSharedPtr &scene,
		                 const CameraSharedPtr &camera);
		void encode_fullscreen_pass(const RenderPassSharedPtr &pass,
		                            const GLMaterialSharedPtr &material);

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

		// Does nothing, and cannot.
		//
		// Kept because 19 scripts call it -- always as
		// psi.renderer:set_msaa_samples(psi.video:get_msaa_samples()), which
		// hands the renderer the value it already has. The sample count is
		// baked into every render pipeline at compile time, so it has to be
		// decided by PSIVideo before any shader is built; by the time a script
		// runs, changing it would invalidate every pipeline.
		void set_msaa_samples(GLint msaa_samples) {
			(void)msaa_samples;
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

		// The last scene's camera matrices, for the full-screen passes that follow
		// it. See the note in encode_fullscreen_pass().
		glm::mat4 _last_view = glm::mat4(1.0f);
		glm::mat4 _last_projection = glm::mat4(1.0f);

		// The frame handle handed to scripts. Created once in init().
		FrameSharedPtr _frame;

		// Shared by render() and encode_pass(): set up the matrix stacks for
		// this camera, sort, cull and draw.
		void draw_scene_in_pass(const RenderSceneSharedPtr &scene,
		                        const RenderContextSharedPtr &ctx,
		                        const CameraSharedPtr &camera,
		                        GLboolean sorting);

		// The video instance reference for accessing the video data and so on.
		shared_ptr<PSIVideo> _video;

		// Metal device, queue and swapchain. Owned by PSIVideo.
		MetalContextSharedPtr _metal_ctx;

		// Offscreen texture we are rendering to.
		//
		// Superseded by PSIRenderTarget, which a pass names directly; this is
		// the single target the old render()/flip() path still uses.
		GLTextureSharedPtr _offscreen_texture;

		// Draw mode sets wireframe and blending together.
		GLint _draw_mode = DrawMode::SHADED;
		// What face side are we culling, or none.
		GLint _cull_mode = CullMode::BACK;

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
		// Viewport size.
		glm::ivec2 _viewport_size = glm::ivec2(0, 0);
};
