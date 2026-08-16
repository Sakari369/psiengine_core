// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// One render pass: where it draws, what it does to the attachments at each end,
// and the state that holds for its whole duration.
//
// Built once at setup and reused every frame, the way MTLRenderPassDescriptor
// is meant to be. Nothing here is per frame.
//
// Before this existed the only pass selector in the engine was a boolean on the
// scene -- PSIRenderScene::set_render_to_texture() -- so the scene carried pass
// state, there was exactly one offscreen target, and every pass cleared. Cull
// mode and fill mode were fields on the renderer, applied once per frame, which
// is why four demos disable face culling scene-wide when only one object needs
// it.
//
// Owning the defaults here also fixes a real bug: PSIRenderObj::draw() used to
// restore fill mode to a hardcoded Fill after drawing a wireframe material, so
// global wireframe silently switched itself off for every later object in the
// frame. Per-object overrides now restore to the pass.

#pragma once

#include "PSIGlobals.h"
#include "PSIMetal.h"
#include "PSIRenderTarget.h"

class PSIRenderPass;
typedef shared_ptr<PSIRenderPass> RenderPassSharedPtr;

class PSIRenderPass {
	public:
		// What happens to the colour attachment when the pass opens.
		enum LoadAction {
			// Start from the clear colour. What every pass did before.
			LOAD_CLEAR = 0,
			// Keep what is already there. This is what makes accumulation and
			// feedback trails expressible.
			LOAD_KEEP = 1,
			// Contents are undefined and the pass will overwrite every pixel.
			// Cheapest, and correct for a full-screen pass.
			LOAD_DISCARD = 2,
		};

		enum CullMode {
			CULL_NONE = 0,
			CULL_FRONT = 1,
			CULL_BACK = 2,
		};

		enum FillMode {
			FILL_SOLID = 0,
			FILL_LINES = 1,
		};

		PSIRenderPass() = default;
		~PSIRenderPass() = default;

		static RenderPassSharedPtr create() {
			return make_shared<PSIRenderPass>();
		}

		// Where this pass draws. Null means the window's drawable.
		void set_target(RenderTargetSharedPtr target) {
			_target = target;
		}
		const RenderTargetSharedPtr &get_target() const {
			return _target;
		}
		bool is_drawable_pass() const {
			return _target == nullptr;
		}

		void set_clear_color(glm::vec4 clear_color) {
			_clear_color = clear_color;
		}
		glm::vec4 get_clear_color() const {
			return _clear_color;
		}

		void set_load_action(GLint action) {
			_load_action = action;
		}
		GLint get_load_action() const {
			return _load_action;
		}

		// Face culling for this pass. Was a renderer-wide setting.
		void set_cull_mode(GLint cull_mode) {
			_cull_mode = cull_mode;
		}
		GLint get_cull_mode() const {
			return _cull_mode;
		}

		// Solid or wireframe for this pass. Per-object material wireframe still
		// overrides it, and now restores back to this rather than to solid.
		void set_fill_mode(GLint fill_mode) {
			_fill_mode = fill_mode;
		}
		GLint get_fill_mode() const {
			return _fill_mode;
		}

		// Depth-sort the scene before drawing it. Off is right for a pass whose
		// contents are known opaque, and for full-screen passes.
		void set_sorting(GLboolean sorting) {
			_sorting = sorting;
		}
		GLboolean get_sorting() const {
			return _sorting;
		}

		// What a pipeline must be built against to draw in this pass. Delegates
		// to the target, or describes the drawable.
		PSIMetal::pass_signature signature() const;

		// Build the pipeline variants every loaded shader needs for this pass,
		// so the first frame that encodes it does not stall compiling them.
		void warm_pipelines() const;

	private:
		// Null = the window's drawable.
		RenderTargetSharedPtr _target;

		glm::vec4 _clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		GLint _load_action = LOAD_CLEAR;
		GLint _cull_mode = CULL_BACK;
		GLint _fill_mode = FILL_SOLID;
		GLboolean _sorting = true;
};
