#include "PSIRenderPass.h"
#include "PSIMetalContext.h"
#include "PSIGLShader.h"

PSIMetal::pass_signature PSIRenderPass::signature() const {
	if (_target != nullptr) {
		return _target->signature();
	}

	// The drawable pass: the swapchain's format, and whatever MSAA level the
	// context is running at.
	PSIMetal::pass_signature sig;
	if (PSI_G::metal_ctx != nullptr) {
		sig.color_format = PSI_G::metal_ctx->color_format();
		sig.depth_format = PSI_G::metal_ctx->depth_format();
		sig.sample_count = (uint32_t)PSI_G::metal_ctx->get_msaa_samples();
	}

	return sig;
}

void PSIRenderPass::warm_pipelines() const {
	// Pipeline creation costs milliseconds. Without this the first frame that
	// encodes a pass compiles one variant per shader it touches, mid-animation.
	const PSIMetal::pass_signature sig = signature();

	for (PSIGLShader *shader : PSIGLShader::all()) {
		shader->warm_pipelines(sig);
	}
}
