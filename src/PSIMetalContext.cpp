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
	if (_capture_cmd != nullptr) {
		_capture_cmd->release();
		_capture_cmd = nullptr;
	}
	if (_capture_texture != nullptr) {
		_capture_texture->release();
		_capture_texture = nullptr;
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

MTL::StorageMode PSIMetalContext::transient_storage_mode() const {
	// Memoryless attachments live only in tile memory: no DRAM is allocated and
	// nothing is written back. Legal only while every pass using them clears on
	// load and DontCare/MultisampleResolve on store, which is what begin_frame()
	// and begin_offscreen_frame() set up. Adding StoreActionStore or
	// LoadActionLoad to depth or the MSAA colour target would fail validation.
	//
	// Tile memory is an Apple-GPU feature; anything else falls back to Private.
	if (_device != nullptr && _device->supportsFamily(MTL::GPUFamilyApple1)) {
		return MTL::StorageModeMemoryless;
	}
	return MTL::StorageModePrivate;
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
	// The depth buffer never leaves the tile: it is cleared at the start of the
	// pass and StoreActionDontCare at the end, so no system memory ever has to
	// back it. Memoryless is what actually expresses that -- Private still
	// allocates DRAM and still writes back.
	desc->setStorageMode(transient_storage_mode());

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
	// Resolved into the drawable inside the tile (StoreActionMultisampleResolve),
	// so the individual samples are never read from memory either.
	color_desc->setStorageMode(transient_storage_mode());

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
		// one -- see the note in the header. The frame's autorelease pool stays
		// open; opening a second one here would leave the first undrained.
		end_encoding();
	} else {
		_frame_pool = NS::AutoreleasePool::alloc()->init();
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
	desc->setStorageMode(transient_storage_mode());
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
		cd->setStorageMode(transient_storage_mode());
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
		_frame_pool = NS::AutoreleasePool::alloc()->init();
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

bool PSIMetalContext::ensure_capture_texture(glm::ivec2 size) {
	if (_device == nullptr || size.x <= 0 || size.y <= 0) {
		return false;
	}
	if (size == _capture_size && _capture_texture != nullptr) {
		return true;
	}

	if (_capture_texture != nullptr) {
		_capture_texture->release();
		_capture_texture = nullptr;
	}

	MTL::TextureDescriptor *desc = MTL::TextureDescriptor::alloc()->init();
	desc->setTextureType(MTL::TextureType2D);
	desc->setPixelFormat(color_format());
	desc->setWidth(static_cast<NS::UInteger>(size.x));
	desc->setHeight(static_cast<NS::UInteger>(size.y));
	desc->setUsage(MTL::TextureUsageShaderRead);
	// Shared so the CPU can read it without an explicit synchronize.
	desc->setStorageMode(MTL::StorageModeShared);

	_capture_texture = _device->newTexture(desc);
	desc->release();

	if (_capture_texture == nullptr) {
		return false;
	}

	_capture_size = size;
	_capture_valid = false;

	return true;
}

bool PSIMetalContext::read_last_frame(std::vector<uint8_t> *rgb, glm::ivec2 *size) {
	if (rgb == nullptr || size == nullptr) {
		return false;
	}
	if (_capture_texture == nullptr || !_capture_valid) {
		psilog_err("No presented frame to read back yet");
		return false;
	}

	// present() committed without waiting, so the blit may still be in flight.
	if (_capture_cmd != nullptr) {
		_capture_cmd->waitUntilCompleted();
	}

	const int w = _capture_size.x;
	const int h = _capture_size.y;

	std::vector<uint8_t> bgra((size_t)w * h * 4);
	MTL::Region region = MTL::Region::Make2D(0, 0, (NS::UInteger)w, (NS::UInteger)h);
	_capture_texture->getBytes(bgra.data(), (NS::UInteger)w * 4, region, 0);

	// The drawable is BGRA; the image writers want tightly packed RGB.
	rgb->resize((size_t)w * h * 3);
	for (size_t i = 0, n = (size_t)w * h; i < n; i++) {
		(*rgb)[i * 3 + 0] = bgra[i * 4 + 2];
		(*rgb)[i * 3 + 1] = bgra[i * 4 + 1];
		(*rgb)[i * 3 + 2] = bgra[i * 4 + 0];
	}

	*size = _capture_size;

	return true;
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

	// Keep a CPU-readable copy of what we are about to show.
	//
	// Core Animation recycles the drawable as soon as it is presented, so there
	// is no equivalent of glReadBuffer(GL_FRONT) to read afterwards -- the copy
	// has to be made now, while the texture is still ours.
	//
	// Only when someone has actually asked for it. This is a full-screen read
	// plus write of the drawable every frame, and the drawable is the
	// supersampled size; unconditionally it was the single largest bandwidth
	// consumer in the engine.
	if (_capture_armed && _drawable != nullptr && ensure_capture_texture(_drawable_size)) {
		MTL::BlitCommandEncoder *blit = _cmd->blitCommandEncoder();
		if (blit != nullptr) {
			blit->copyFromTexture(_drawable->texture(), 0, 0,
			                      MTL::Origin(0, 0, 0),
			                      MTL::Size((NS::UInteger)_drawable_size.x,
			                                (NS::UInteger)_drawable_size.y, 1),
			                      _capture_texture, 0, 0,
			                      MTL::Origin(0, 0, 0));
			blit->endEncoding();
			_capture_valid = true;

			// Hold this buffer past the pool drain so read_last_frame() has
			// something to wait on -- commit() below does not block.
			if (_capture_cmd != nullptr) {
				_capture_cmd->release();
			}
			_capture_cmd = _cmd->retain();
		}
	}

	// An offscreen frame has no drawable; it still needs committing.
	if (_drawable != nullptr) {
		_cmd->presentDrawable(_drawable);
	}

	// Release the frame slot once the GPU is actually done with this frame, and
	// record how long the GPU spent on it. The context outlives every frame --
	// shutdown() drains all of them before releasing anything -- so capturing
	// `this` here is safe even though the handler runs on a Metal thread.
	dispatch_semaphore_t sem = _frame_sem;
	_cmd->addCompletedHandler([sem, this](MTL::CommandBuffer *cmd) {
		double seconds = cmd->GPUEndTime() - cmd->GPUStartTime();
		if (seconds > 0.0) {
			_gpu_time_ns.fetch_add((uint64_t)(seconds * 1e9), std::memory_order_relaxed);
			_gpu_frames.fetch_add(1, std::memory_order_relaxed);
		}
		dispatch_semaphore_signal(sem);
	});

	_cmd->commit();

	_cmd = nullptr;
	_drawable = nullptr;
	_frame_started = false;

	// Move to the next in-flight slot. Here rather than in begin_frame() so a
	// frame that render()s twice keeps writing into one slot.
	_frame_counter++;

	// Everything autoreleased while encoding goes now.
	if (_frame_pool != nullptr) {
		_frame_pool->release();
		_frame_pool = nullptr;
	}
}

double PSIMetalContext::gpu_time_mean_ms() const {
	uint32_t frames = _gpu_frames.load(std::memory_order_relaxed);
	if (frames == 0) {
		return 0.0;
	}
	uint64_t total_ns = _gpu_time_ns.load(std::memory_order_relaxed);
	return (double)total_ns / (double)frames / 1e6;
}

void PSIMetalContext::arm_capture() {
	if (_capture_armed) {
		return;
	}

	_capture_armed = true;

	// The drawable cannot be a blit source while the layer is framebuffer-only.
	// This applies to drawables vended from now on, which is why the frame that
	// arms capture is not itself readable.
	PSIMetal::set_layer_framebuffer_only(_layer, false);

	psilog(PSILog::EXPORT,
	       "Frame capture armed; the next presented frame will be readable");
}
