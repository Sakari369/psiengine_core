#include "PSIRenderTarget.h"
#include "PSIMetalContext.h"

PSIRenderTarget::~PSIRenderTarget() {
	release_attachments();
}

void PSIRenderTarget::release_attachments() {
	if (_msaa_color != nullptr) {
		_msaa_color->release();
		_msaa_color = nullptr;
	}
	if (_owns_depth_attachment && _depth_attachment != nullptr) {
		_depth_attachment->release();
	}
	_depth_attachment = nullptr;
	_owns_depth_attachment = false;

	_color = nullptr;
	_depth = nullptr;
}

bool PSIRenderTarget::init(glm::ivec2 size, GLuint pixel_format, GLint samples, GLint depth_mode) {
	if (PSI_G::metal_ctx == nullptr || PSI_G::metal_ctx->device() == nullptr) {
		psilog_err("No Metal device when creating render target");
		return false;
	}
	if (size.x <= 0 || size.y <= 0) {
		psilog_err("Render target needs a positive size, got %dx%d", size.x, size.y);
		return false;
	}

	MTL::Device *device = PSI_G::metal_ctx->device();

	if (samples < 1) {
		samples = 1;
	}
	// Step down to what the device actually supports, as the swapchain path
	// does, rather than failing to allocate.
	while (samples > 1 && !device->supportsTextureSampleCount(samples)) {
		samples /= 2;
	}

	release_attachments();

	_size = size;
	_pixel_format = pixel_format;
	_samples = samples;
	_depth_mode = depth_mode;

	// Sampleable colour. Single-sampled even when the pass is multisampled --
	// it is the resolve target in that case.
	_color = PSIGLTexture::create();
	if (!_color->create_render_target(size.x, size.y, pixel_format, 1)) {
		_color = nullptr;
		return false;
	}

	if (samples > 1) {
		MTL::TextureDescriptor *desc = MTL::TextureDescriptor::alloc()->init();
		desc->setTextureType(MTL::TextureType2DMultisample);
		desc->setSampleCount(static_cast<NS::UInteger>(samples));
		desc->setPixelFormat((MTL::PixelFormat)pixel_format);
		desc->setWidth(static_cast<NS::UInteger>(size.x));
		desc->setHeight(static_cast<NS::UInteger>(size.y));
		desc->setUsage(MTL::TextureUsageRenderTarget);
		// Resolved into _color inside the tile; the samples themselves are
		// never read from memory.
		desc->setStorageMode(PSI_G::metal_ctx->transient_storage_mode());

		_msaa_color = device->newTexture(desc);
		desc->release();

		if (_msaa_color == nullptr) {
			psilog_err("Failed creating %dx MSAA colour attachment for %dx%d target",
			           samples, size.x, size.y);
			_color = nullptr;
			return false;
		}
	}

	if (depth_mode == DEPTH_SAMPLE) {
		// Real memory, so the pass can store it and a later pass can read it.
		_depth = PSIGLTexture::create();
		if (!_depth->create_render_target(size.x, size.y,
		                                  (GLuint)PSI_G::metal_ctx->depth_format(), samples)) {
			psilog_err("Failed creating sampleable depth for %dx%d target", size.x, size.y);
			_depth = nullptr;
			_color = nullptr;
			return false;
		}
		_depth_attachment = _depth->get_metal_texture();
		_owns_depth_attachment = false;
	} else if (depth_mode == DEPTH_TRANSIENT) {
		// Cleared at the start of the pass, DontCare at the end, so it never
		// needs to leave tile memory.
		MTL::TextureDescriptor *desc = MTL::TextureDescriptor::alloc()->init();
		desc->setTextureType(samples > 1 ? MTL::TextureType2DMultisample : MTL::TextureType2D);
		desc->setSampleCount(static_cast<NS::UInteger>(samples));
		desc->setPixelFormat(PSI_G::metal_ctx->depth_format());
		desc->setWidth(static_cast<NS::UInteger>(size.x));
		desc->setHeight(static_cast<NS::UInteger>(size.y));
		desc->setUsage(MTL::TextureUsageRenderTarget);
		desc->setStorageMode(PSI_G::metal_ctx->transient_storage_mode());

		_depth_attachment = device->newTexture(desc);
		desc->release();

		if (_depth_attachment == nullptr) {
			psilog_err("Failed creating depth attachment for %dx%d target", size.x, size.y);
			_color = nullptr;
			return false;
		}
		_owns_depth_attachment = true;
	}

	psilog(PSILog::INIT, "Render target %dx%d, format %u, %d samples, depth %s",
	       size.x, size.y, pixel_format, samples,
	       depth_mode == DEPTH_SAMPLE ? "sampleable" :
	       depth_mode == DEPTH_NONE ? "none" : "transient");

	return true;
}

PSIMetal::pass_signature PSIRenderTarget::signature() const {
	PSIMetal::pass_signature sig;
	sig.color_format = (MTL::PixelFormat)_pixel_format;
	// A pass with no depth attachment must declare PixelFormatInvalid, or
	// pipeline creation rejects it.
	sig.depth_format = (_depth_mode == DEPTH_NONE)
		? MTL::PixelFormatInvalid
		: PSI_G::metal_ctx->depth_format();
	sig.sample_count = (uint32_t)_samples;

	return sig;
}

MTL::Texture *PSIRenderTarget::color_attachment() const {
	// Multisampled passes render into the MSAA texture and resolve; others
	// render straight into the sampleable one.
	if (_msaa_color != nullptr) {
		return _msaa_color;
	}
	return (_color != nullptr) ? _color->get_metal_texture() : nullptr;
}

MTL::Texture *PSIRenderTarget::resolve_attachment() const {
	if (_msaa_color == nullptr) {
		return nullptr;
	}
	return (_color != nullptr) ? _color->get_metal_texture() : nullptr;
}
