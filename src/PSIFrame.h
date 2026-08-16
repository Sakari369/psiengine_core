// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// One frame's worth of encoding: open it, encode passes into it, present.
//
// This is what the frame looks like from a script:
//
//     local frame = psi.renderer:begin_frame()
//     frame:encode(offscreen_pass, scene, camera)
//     frame:encode(screen_pass, tonemap_material)
//     frame:present()
//
// It replaces psi.renderer:render(scene, ctx, camera) followed by
// psi.video:flip(). That pair had no object for the frame and none for the
// pass, so the frame was split across PSIGLRenderer and PSIVideo, the drawable
// had to be acquired lazily to survive a script rendering twice, and a second
// render() silently cleared away the first one's work.
//
// The frame does not own the command buffer -- PSIMetalContext does, because
// PSIVideo tears that down. This is a handle, created once by the renderer and
// reused every frame, so the loop allocates nothing.

#pragma once

#include "PSIGlobals.h"
#include "PSIRenderPass.h"
#include "PSIRenderScene.h"
#include "PSIRenderContext.h"
#include "PSICamera.h"

class PSIGLRenderer;

class PSIFrame;
typedef shared_ptr<PSIFrame> FrameSharedPtr;

class PSIFrame {
	public:
		PSIFrame() = default;
		~PSIFrame() = default;

		static FrameSharedPtr create(PSIGLRenderer *renderer) {
			FrameSharedPtr frame = make_shared<PSIFrame>();
			frame->_renderer = renderer;
			return frame;
		}

		// Draw a scene into a pass. The camera supplies the view and projection
		// for this pass only, so two passes can see the scene from two places.
		void encode(const RenderPassSharedPtr &pass,
		            const RenderSceneSharedPtr &scene,
		            const CameraSharedPtr &camera);

		// Draw one full-screen triangle into a pass with the given material.
		//
		// No scene, no camera and no mesh: the vertex shader generates the
		// triangle from vertex_id. That is how a post-processing pass reads the
		// target an earlier pass wrote, and it avoids needing an orthographic
		// camera, which PSICamera does not have.
		void encode_fullscreen(const RenderPassSharedPtr &pass,
		                       const GLMaterialSharedPtr &material);

		// Finish the frame: close the open pass, present the drawable if one
		// was acquired, and commit.
		void present();

		// How many passes have been encoded since the frame opened. Used by the
		// per-frame buffer rotation to spot a mesh written twice in one frame.
		GLuint get_pass_index() const { return _pass_index; }

	private:
		// Non-owning: the renderer outlives every frame handle it hands out.
		PSIGLRenderer *_renderer = nullptr;
		GLuint _pass_index = 0;

		friend class PSIGLRenderer;
};
