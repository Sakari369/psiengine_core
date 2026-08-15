#include "PSIMetalContext.h"
#include "PSIMetalLayer.h"

PSIMetalContext::~PSIMetalContext() {
	shutdown();
}

bool PSIMetalContext::init(GLFWwindow *window, glm::ivec2 drawable_size, double contents_scale) {
	_device = MTL::CreateSystemDefaultDevice();
	if (_device == nullptr) {
		psilog_err("No Metal device available");
		return false;
	}

	_queue = _device->newCommandQueue();
	if (_queue == nullptr) {
		psilog_err("Failed creating Metal command queue");
		return false;
	}

	_layer = PSIMetal::attach_metal_layer(window, _device, contents_scale);
	if (_layer == nullptr) {
		psilog_err("Failed attaching CAMetalLayer to window");
		return false;
	}

	_frame_sem = dispatch_semaphore_create(PSIMetal::MAX_FRAMES_IN_FLIGHT);

	if (!create_depth_states()) {
		psilog_err("Failed creating depth stencil states");
		return false;
	}

	resize(drawable_size);

	psilog(PSILog::INIT, "%s", get_device_info_str().c_str());

	return true;
}

void PSIMetalContext::shutdown() {
	// Do not tear down while the GPU still has frames referencing our resources.
	if (_frame_sem != nullptr) {
		for (int i = 0; i < PSIMetal::MAX_FRAMES_IN_FLIGHT; i++) {
			dispatch_semaphore_wait(_frame_sem, DISPATCH_TIME_FOREVER);
		}
		_frame_sem = nullptr;
	}

	if (_msaa_texture != nullptr) {
		_msaa_texture->release();
		_msaa_texture = nullptr;
	}
	if (_offscreen_depth != nullptr) {
		_offscreen_depth->release();
		_offscreen_depth = nullptr;
	}
	if (_offscreen_msaa != nullptr) {
		_offscreen_msaa->release();
		_offscreen_msaa = nullptr;
	}
	if (_depth_state_on != nullptr) {
		_depth_state_on->release();
		_depth_state_on = nullptr;
	}
	if (_depth_state_off != nullptr) {
		_depth_state_off->release();
		_depth_state_off = nullptr;
	}
	if (_shader_library != nullptr) {
		_shader_library->release();
		_shader_library = nullptr;
	}
	if (_depth_texture != nullptr) {
		_depth_texture->release();
		_depth_texture = nullptr;
	}
	if (_queue != nullptr) {
		_queue->release();
		_queue = nullptr;
	}
	if (_device != nullptr) {
		_device->release();
		_device = nullptr;
	}
	_layer = nullptr;
}

std::string PSIMetalContext::get_device_info_str() const {
	if (_device == nullptr) {
		return "Metal: no device";
	}

	std::string name = _device->name()->utf8String();
	std::string info = "Metal device: " + name;

	if (_device->supportsFamily(MTL::GPUFamilyApple7)) {
		info += " (Apple7+)";
	} else if (_device->supportsFamily(MTL::GPUFamilyApple1)) {
		info += " (Apple)";
	} else if (_device->supportsFamily(MTL::GPUFamilyMac2)) {
		info += " (Mac2)";
	}

	return info;
}

MTL::Library *PSIMetalContext::shader_library() {
	if (_shader_library != nullptr || _shader_library_tried) {
		return _shader_library;
	}
	_shader_library_tried = true;

	if (_device == nullptr) {
		return nullptr;
	}

	// The metallib is emitted next to the executable by the build, so derive its
	// path from argv[0] rather than from the asset dir -- it is a build product,
	// not an asset.
	std::string exe_path = PSI_G::program_name != nullptr ? PSI_G::program_name : "";
	std::string dir = ".";
	size_t slash = exe_path.find_last_of('/');
	if (slash != std::string::npos) {
		dir = exe_path.substr(0, slash);
	}
	std::string lib_path = dir + "/psishaders.metallib";

	NS::String *ns_path = NS::String::string(lib_path.c_str(), NS::UTF8StringEncoding);
	NS::Error *error = nullptr;
	_shader_library = _device->newLibrary(ns_path, &error);

	if (_shader_library == nullptr) {
		const char *msg = "unknown error";
		if (error != nullptr && error->localizedDescription() != nullptr) {
			msg = error->localizedDescription()->utf8String();
		}
		psilog_err("Failed loading shader library \"%s\": %s", lib_path.c_str(), msg);
		return nullptr;
	}

	psilog(PSILog::INIT, "Loaded shader library %s", lib_path.c_str());

	return _shader_library;
}

bool PSIMetalContext::create_depth_states() {
	if (_device == nullptr) {
		return false;
	}

	MTL::DepthStencilDescriptor *desc = MTL::DepthStencilDescriptor::alloc()->init();

	// Matches PSIGLRenderer::init(): glEnable(GL_DEPTH_TEST) + glDepthFunc(GL_LESS).
	desc->setDepthCompareFunction(MTL::CompareFunctionLess);
	desc->setDepthWriteEnabled(true);
	_depth_state_on = _device->newDepthStencilState(desc);

	// glDisable(GL_DEPTH_TEST): always passes and stops writing depth. Used by
	// skyboxes and UI elements via PSIRenderObj::set_depth_tested(false).
	desc->setDepthCompareFunction(MTL::CompareFunctionAlways);
	desc->setDepthWriteEnabled(false);
	_depth_state_off = _device->newDepthStencilState(desc);

	desc->release();

	return _depth_state_on != nullptr && _depth_state_off != nullptr;
}

void PSIMetalContext::set_depth_test_enabled(bool enabled) {
	if (_encoder == nullptr) {
		return;
	}

	MTL::DepthStencilState *state = enabled ? _depth_state_on : _depth_state_off;
	if (state != nullptr) {
		_encoder->setDepthStencilState(state);
	}
}

void PSIMetalContext::set_msaa_samples(int samples) {
	if (_device == nullptr) {
		return;
	}

	if (samples < 1) {
		samples = 1;
	}

	// Metal only accepts specific counts, and support varies by device. Step
	// down to the highest supported count at or below what was asked for, so a
	// script requesting 8 on hardware that tops out at 4 still gets MSAA.
	while (samples > 1 && !_device->supportsTextureSampleCount(samples)) {
		samples /= 2;
	}

	if (samples == _msaa_samples) {
		return;
	}

	_msaa_samples = samples;
	psilog(PSILog::INIT, "MSAA sample count set to %d", _msaa_samples);

	// Rebuild the render targets at the new sample count.
	create_depth_texture(_drawable_size);
}

bool PSIMetalContext::create_depth_texture(glm::ivec2 size) {
	if (_device == nullptr || size.x <= 0 || size.y <= 0) {
		return false;
	}

	if (_depth_texture != nullptr) {
		_depth_texture->release();
		_depth_texture = nullptr;
	}
	if (_msaa_texture != nullptr) {
		_msaa_texture->release();
		_msaa_texture = nullptr;
	}

	const bool multisampled = _msaa_samples > 1;

	MTL::TextureDescriptor *desc = MTL::TextureDescriptor::alloc()->init();
	desc->setTextureType(multisampled ? MTL::TextureType2DMultisample : MTL::TextureType2D);
	desc->setSampleCount(static_cast<NS::UInteger>(_msaa_samples));
	desc->setPixelFormat(depth_format());
	desc->setWidth(static_cast<NS::UInteger>(size.x));
	desc->setHeight(static_cast<NS::UInteger>(size.y));
	desc->setUsage(MTL::TextureUsageRenderTarget);
	// The depth buffer is never read back on the CPU and does not need to
	// survive past the frame, so keep it in tile/private memory.
	desc->setStorageMode(MTL::StorageModePrivate);

	_depth_texture = _device->newTexture(desc);
	desc->release();

	if (_depth_texture == nullptr) {
		psilog_err("Failed creating depth texture %dx%d", size.x, size.y);
		return false;
	}

	if (!multisampled) {
		return true;
	}

	// Multisampled colour target. Rendering goes here and is resolved into the
	// drawable when the pass ends, which is the work GLFW_SAMPLES used to hide.
	MTL::TextureDescriptor *color_desc = MTL::TextureDescriptor::alloc()->init();
	color_desc->setTextureType(MTL::TextureType2DMultisample);
	color_desc->setSampleCount(static_cast<NS::UInteger>(_msaa_samples));
	color_desc->setPixelFormat(color_format());
	color_desc->setWidth(static_cast<NS::UInteger>(size.x));
	color_desc->setHeight(static_cast<NS::UInteger>(size.y));
	color_desc->setUsage(MTL::TextureUsageRenderTarget);
	color_desc->setStorageMode(MTL::StorageModePrivate);

	_msaa_texture = _device->newTexture(color_desc);
	color_desc->release();

	if (_msaa_texture == nullptr) {
		psilog_err("Failed creating %dx MSAA target %dx%d", _msaa_samples, size.x, size.y);
		return false;
	}

	return true;
}

void PSIMetalContext::set_supersample_factor(int factor) {
	if (factor < 1) {
		factor = 1;
	}
	if (factor == _supersample) {
		return;
	}

	_supersample = factor;
	psilog(PSILog::INIT, "Supersampling set to %dx", _supersample);

	// Re-apply at the new factor.
	glm::ivec2 logical = _logical_size;
	_logical_size = glm::ivec2(0, 0);
	resize(logical);
}

void PSIMetalContext::resize(glm::ivec2 logical_size) {
	if (logical_size.x <= 0 || logical_size.y <= 0) {
		return;
	}
	if (logical_size == _logical_size && _depth_texture != nullptr) {
		return;
	}

	_logical_size = logical_size;

	// Draw at supersample^2 the pixel count; the layer minifies it back to the
	// window on composite, which is where the extra antialiasing comes from.
	_drawable_size = logical_size * _supersample;

	PSIMetal::set_layer_drawable_size(_layer, _drawable_size.x, _drawable_size.y);
	create_depth_texture(_drawable_size);

	if (_supersample > 1) {
		psilog(PSILog::INIT, "Rendering at %dx%d for a %dx%d window (%dx supersampled)",
		       _drawable_size.x, _drawable_size.y,
		       logical_size.x, logical_size.y, _supersample);
	}
}

void PSIMetalContext::set_vsync(bool enabled) {
	PSIMetal::set_layer_display_sync(_layer, enabled);
}

MTL::RenderCommandEncoder *PSIMetalContext::begin_frame(const glm::vec4 &clear_color) {
	if (_device == nullptr || _layer == nullptr) {
		return nullptr;
	}

	if (_frame_started) {
		// Second pass in the same frame. Close the current one and start a fresh
		// one -- see the note in the header.
		end_encoding();
	} else {
		dispatch_semaphore_wait(_frame_sem, DISPATCH_TIME_FOREVER);
		_cmd = _queue->commandBuffer();
		_frame_started = true;
	}

	// Acquire the drawable lazily, and only once per frame. It may already be
	// held (a second render() call) or still be null because the frame was
	// opened by an offscreen pass that never needed one.
	if (_drawable == nullptr) {
		CA::MetalLayer *layer = reinterpret_cast<CA::MetalLayer *>(_layer);
		_drawable = layer->nextDrawable();

		if (_drawable == nullptr) {
			// Occluded or minimized -- nothing to draw into. present() still
			// owns the frame slot and will commit and release it.
			return nullptr;
		}
	}

	MTL::RenderPassDescriptor *pass = MTL::RenderPassDescriptor::alloc()->init();

	MTL::RenderPassColorAttachmentDescriptor *color = pass->colorAttachments()->object(0);
	color->setLoadAction(MTL::LoadActionClear);
	color->setClearColor(MTL::ClearColor::Make(clear_color.r, clear_color.g,
	                                           clear_color.b, clear_color.a));

	if (_msaa_texture != nullptr) {
		// Render into the multisampled target and resolve into the drawable as
		// the pass ends. MultisampleResolve rather than StoreAndMultisampleResolve
		// because nothing reads the multisampled samples afterwards.
		color->setTexture(_msaa_texture);
		color->setResolveTexture(_drawable->texture());
		color->setStoreAction(MTL::StoreActionMultisampleResolve);
	} else {
		color->setTexture(_drawable->texture());
		color->setStoreAction(MTL::StoreActionStore);
	}

	if (_depth_texture != nullptr) {
		MTL::RenderPassDepthAttachmentDescriptor *depth = pass->depthAttachment();
		depth->setTexture(_depth_texture);
		depth->setLoadAction(MTL::LoadActionClear);
		// Nothing reads the depth buffer after the frame ends.
		depth->setStoreAction(MTL::StoreActionDontCare);
		depth->setClearDepth(1.0);
	}

	_encoder = _cmd->renderCommandEncoder(pass);
	pass->release();

	if (_encoder == nullptr) {
		psilog_err("Failed creating render command encoder");
		return nullptr;
	}

	// Match the OpenGL defaults the engine relied on: counter-clockwise front
	// faces (Metal defaults to clockwise) and a viewport covering the drawable.
	_encoder->setFrontFacingWinding(MTL::WindingCounterClockwise);
	_encoder->setViewport(MTL::Viewport{
		0.0, 0.0,
		static_cast<double>(_drawable_size.x),
		static_cast<double>(_drawable_size.y),
		0.0, 1.0
	});

	// Depth testing on by default, as PSIGLRenderer::init() did with
	// glEnable(GL_DEPTH_TEST). Metal's default would be always-pass with depth
	// writes off, which renders in draw order instead of depth order.
	set_depth_test_enabled(true);

	// A new encoder starts with no pipeline bound, so nothing may draw until a
	// shader binds one.
	_current_shader = nullptr;

	return _encoder;
}

bool PSIMetalContext::ensure_offscreen_targets(glm::ivec2 size) {
	if (_device == nullptr || size.x <= 0 || size.y <= 0) {
		return false;
	}
	if (size == _offscreen_size && _offscreen_depth != nullptr) {
		return true;
	}

	if (_offscreen_depth != nullptr) {
		_offscreen_depth->release();
		_offscreen_depth = nullptr;
	}
	if (_offscreen_msaa != nullptr) {
		_offscreen_msaa->release();
		_offscreen_msaa = nullptr;
	}

	const bool multisampled = _msaa_samples > 1;

	MTL::TextureDescriptor *desc = MTL::TextureDescriptor::alloc()->init();
	desc->setTextureType(multisampled ? MTL::TextureType2DMultisample : MTL::TextureType2D);
	desc->setSampleCount(static_cast<NS::UInteger>(_msaa_samples));
	desc->setPixelFormat(depth_format());
	desc->setWidth(static_cast<NS::UInteger>(size.x));
	desc->setHeight(static_cast<NS::UInteger>(size.y));
	desc->setUsage(MTL::TextureUsageRenderTarget);
	desc->setStorageMode(MTL::StorageModePrivate);
	_offscreen_depth = _device->newTexture(desc);
	desc->release();

	if (_offscreen_depth == nullptr) {
		psilog_err("Failed creating offscreen depth target %dx%d", size.x, size.y);
		return false;
	}

	if (multisampled) {
		MTL::TextureDescriptor *cd = MTL::TextureDescriptor::alloc()->init();
		cd->setTextureType(MTL::TextureType2DMultisample);
		cd->setSampleCount(static_cast<NS::UInteger>(_msaa_samples));
		cd->setPixelFormat(color_format());
		cd->setWidth(static_cast<NS::UInteger>(size.x));
		cd->setHeight(static_cast<NS::UInteger>(size.y));
		cd->setUsage(MTL::TextureUsageRenderTarget);
		cd->setStorageMode(MTL::StorageModePrivate);
		_offscreen_msaa = _device->newTexture(cd);
		cd->release();

		if (_offscreen_msaa == nullptr) {
			psilog_err("Failed creating offscreen MSAA target %dx%d", size.x, size.y);
			return false;
		}
	}

	_offscreen_size = size;
	psilog(PSILog::INIT, "Offscreen targets ready at %dx%d", size.x, size.y);

	return true;
}

MTL::RenderCommandEncoder *PSIMetalContext::begin_offscreen_frame(MTL::Texture *target,
                                                                  const glm::vec4 &clear_color) {
	if (_device == nullptr || target == nullptr) {
		return nullptr;
	}

	glm::ivec2 size(static_cast<int>(target->width()), static_cast<int>(target->height()));
	if (!ensure_offscreen_targets(size)) {
		return nullptr;
	}

	if (_frame_started) {
		// Second pass this frame; close the previous one and reuse the command
		// buffer, as the on-screen path does.
		end_encoding();
	} else {
		dispatch_semaphore_wait(_frame_sem, DISPATCH_TIME_FOREVER);
		_cmd = _queue->commandBuffer();
		_frame_started = true;
		// No drawable: present() will commit without presenting.
		_drawable = nullptr;
	}

	MTL::RenderPassDescriptor *pass = MTL::RenderPassDescriptor::alloc()->init();

	MTL::RenderPassColorAttachmentDescriptor *color = pass->colorAttachments()->object(0);
	color->setLoadAction(MTL::LoadActionClear);
	color->setClearColor(MTL::ClearColor::Make(clear_color.r, clear_color.g,
	                                           clear_color.b, clear_color.a));
	if (_offscreen_msaa != nullptr) {
		color->setTexture(_offscreen_msaa);
		color->setResolveTexture(target);
		color->setStoreAction(MTL::StoreActionMultisampleResolve);
	} else {
		color->setTexture(target);
		color->setStoreAction(MTL::StoreActionStore);
	}

	MTL::RenderPassDepthAttachmentDescriptor *depth = pass->depthAttachment();
	depth->setTexture(_offscreen_depth);
	depth->setLoadAction(MTL::LoadActionClear);
	depth->setStoreAction(MTL::StoreActionDontCare);
	depth->setClearDepth(1.0);

	_encoder = _cmd->renderCommandEncoder(pass);
	pass->release();

	if (_encoder == nullptr) {
		psilog_err("Failed creating offscreen render encoder");
		return nullptr;
	}

	_encoder->setFrontFacingWinding(MTL::WindingCounterClockwise);
	_encoder->setViewport(MTL::Viewport{
		0.0, 0.0,
		static_cast<double>(size.x), static_cast<double>(size.y),
		0.0, 1.0
	});
	set_depth_test_enabled(true);
	_current_shader = nullptr;

	return _encoder;
}

void PSIMetalContext::end_encoding() {
	if (_encoder != nullptr) {
		_encoder->endEncoding();
		_encoder = nullptr;
	}
}

void PSIMetalContext::present() {
	if (!_frame_started) {
		return;
	}

	end_encoding();

	// An offscreen frame has no drawable; it still needs committing.
	if (_drawable != nullptr) {
		_cmd->presentDrawable(_drawable);
	}

	// Release the frame slot once the GPU is actually done with this frame.
	dispatch_semaphore_t sem = _frame_sem;
	_cmd->addCompletedHandler([sem](MTL::CommandBuffer *) {
		dispatch_semaphore_signal(sem);
	});

	_cmd->commit();

	_cmd = nullptr;
	_drawable = nullptr;
	_frame_started = false;
}
