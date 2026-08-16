#include "PSIFrame.h"
#include "PSIGLRenderer.h"
#include "PSIMetalContext.h"

void PSIFrame::encode(const RenderPassSharedPtr &pass,
                      const RenderSceneSharedPtr &scene,
                      const CameraSharedPtr &camera) {
	if (_renderer == nullptr) {
		return;
	}

	_renderer->encode_pass(pass, scene, camera);
	_pass_index++;
}

void PSIFrame::encode_fullscreen(const RenderPassSharedPtr &pass,
                                 const GLMaterialSharedPtr &material) {
	if (_renderer == nullptr) {
		return;
	}

	_renderer->encode_fullscreen_pass(pass, material);
	_pass_index++;
}

void PSIFrame::present() {
	if (_renderer != nullptr) {
		_renderer->end_frame();
	}
	_pass_index = 0;
}
