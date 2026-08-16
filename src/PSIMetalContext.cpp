#include "PSIMetalContext.h"
#include "PSIMetalLayer.h"
#include "PSIRenderPass.h"

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

	// Held for edr_headroom(), which resolves the window's current screen.
	_window = window;

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
	if (_depth_state_read_only != nullptr) {
		_depth_state_read_only->release();
		_depth_state_read_only = nullptr;
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

	// glDepthMask(GL_FALSE) with the test still on: what a transparent object
	// needs, and what neither state above provides.
	//
	// A blended object must still be hidden by opaque geometry in front of it,
	// so the test stays. But it must not write depth, because the blend result
	// depends on draw order and a transparent fragment that writes depth
	// rejects whatever is behind it -- which does not blend it, it deletes it.
	// Inside one instanced draw that is unavoidable otherwise: the instances
	// come off the buffer in a fixed order that has nothing to do with their
	// distance, so with writes on, whichever instance happens to be drawn first
	// punches a hole through the ones behind it.
	desc->setDepthCompareFunction(MTL::CompareFunctionLess);
	desc->setDepthWriteEnabled(false);
	_depth_state_read_only = _device->newDepthStencilState(desc);

	desc->release();

	return _depth_state_on != nullptr
	    && _depth_state_off != nullptr
	    && _depth_state_read_only != nullptr;
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

void PSIMetalContext::set_depth_write_enabled(bool enabled) {
	if (_encoder == nullptr) {
		return;
	}

	// Only meaningful while the test is on. With the test off there is nothing
	// to read and nothing is written either way, so the caller gets what it
	// already had.
	if (_pass_depth_test == false) {
		return;
	}

	MTL::DepthStencilState *state = enabled ? _depth_state_on : _depth_state_read_only;
	if (state != nullptr) {
		_encoder->setDepthStencilState(state);
	}
}

void PSIMetalContext::set_pass_depth_test(bool enabled) {
	_pass_depth_test = enabled;
	set_depth_test_enabled(enabled);
}

void PSIMetalContext::set_fill_mode(bool lines) {
	if (_encoder == nullptr) {
		return;
	}

	_encoder->setTriangleFillMode(lines
		? MTL::TriangleFillModeLines
		: MTL::TriangleFillModeFill);
}

void PSIMetalContext::set_pass_fill_mode(bool lines) {
	_pass_fill_lines = lines;
	set_fill_mode(lines);
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

	// Keep the signature current outside a pass, so a shader compiled at load
	// time builds its first pipeline against what the drawable pass will use.
	update_pass_signature(nullptr);

	// Rebuild the render targets at the new sample count.
	create_depth_texture(_drawable_size);
}

void PSIMetalContext::update_pass_signature(MTL::Texture *color_target) {
	// The sample count comes from the multisampled attachment when there is
	// one, because that is what the pipeline has to declare -- not from the
	// resolve target, which is always single-sampled.
	_pass_signature.color_format = (color_target != nullptr)
		? color_target->pixelFormat()
		: color_format();
	_pass_signature.depth_format = depth_format();
	_pass_signature.sample_count = (uint32_t)_msaa_samples;
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

void PSIMetalContext::open_or_continue_frame() {
	if (_frame_started) {
		// Another pass in the same frame. Close the one already open and reuse
		// the command buffer and the autorelease pool -- opening a second pool
		// here would leave the first undrained.
		end_encoding();
		return;
	}

	_frame_pool = NS::AutoreleasePool::alloc()->init();
	dispatch_semaphore_wait(_frame_sem, DISPATCH_TIME_FOREVER);
	_cmd = _queue->commandBuffer();
	_frame_started = true;
}

MTL::RenderCommandEncoder *PSIMetalContext::begin_pass(const PSIRenderPass &pass) {
	if (_device == nullptr) {
		return nullptr;
	}

	const RenderTargetSharedPtr &target = pass.get_target();
	const bool to_drawable = (target == nullptr);

	if (to_drawable && _layer == nullptr) {
		return nullptr;
	}
	if (!to_drawable && !target->is_valid()) {
		psilog_err("Render pass targets an unallocated render target");
		return nullptr;
	}

	open_or_continue_frame();

	glm::ivec2 viewport_size;

	if (to_drawable) {
		// Acquire the drawable lazily, and only once per frame. It may already
		// be held by an earlier pass, or still be null because the frame was
		// opened by an offscreen pass that never needed one.
		if (_drawable == nullptr) {
			CA::MetalLayer *layer = reinterpret_cast<CA::MetalLayer *>(_layer);
			_drawable = layer->nextDrawable();

			if (_drawable == nullptr) {
				// Occluded or minimized. present() still owns the frame slot
				// and will commit and release it.
				return nullptr;
			}
		}
		viewport_size = _drawable_size;
	} else {
		viewport_size = target->get_size();
	}

	MTL::RenderPassDescriptor *desc = MTL::RenderPassDescriptor::alloc()->init();

	// Is this pass multisampled? If so its colour attachment is the
	// multisampled texture, which is memoryless and resolves into the real one
	// as the pass ends.
	const bool multisampled = to_drawable
		? (_msaa_texture != nullptr)
		: (target->resolve_attachment() != nullptr);

	MTL::LoadAction load = MTL::LoadActionClear;
	if (pass.get_load_action() == PSIRenderPass::LOAD_KEEP) {
		if (multisampled) {
			// Cannot be honoured, and honouring it halfway would be worse than
			// refusing: the attachment being loaded is the MSAA texture, which
			// lives only in tile memory and is resolved away at the end of
			// every pass. There is nothing there to load, and asking anyway is
			// a validation error.
			//
			// Keeping contents needs a single-sampled pass -- which is what an
			// accumulation or feedback pass wants regardless, since it reads
			// its own previous output.
			static bool warned = false;
			if (!warned) {
				warned = true;
				psilog_err("Render pass asked to keep its contents but is "
				           "multisampled; its colour attachment is memoryless "
				           "and has nothing to keep. Clearing instead -- give "
				           "the pass a render target with samples = 1.");
			}
		} else {
			load = MTL::LoadActionLoad;
		}
	} else if (pass.get_load_action() == PSIRenderPass::LOAD_DISCARD) {
		load = MTL::LoadActionDontCare;
	}

	const glm::vec4 clear = pass.get_clear_color();

	MTL::RenderPassColorAttachmentDescriptor *color = desc->colorAttachments()->object(0);
	color->setLoadAction(load);
	color->setClearColor(MTL::ClearColor::Make(clear.r, clear.g, clear.b, clear.a));

	bool has_depth = false;

	if (to_drawable) {
		if (_msaa_texture != nullptr) {
			color->setTexture(_msaa_texture);
			color->setResolveTexture(_drawable->texture());
			color->setStoreAction(MTL::StoreActionMultisampleResolve);
			update_pass_signature(_msaa_texture);
		} else {
			color->setTexture(_drawable->texture());
			color->setStoreAction(MTL::StoreActionStore);
			update_pass_signature(_drawable->texture());
		}

		has_depth = (_depth_texture != nullptr);
		if (_depth_texture != nullptr) {
			MTL::RenderPassDepthAttachmentDescriptor *depth = desc->depthAttachment();
			depth->setTexture(_depth_texture);
			depth->setLoadAction(MTL::LoadActionClear);
			depth->setStoreAction(MTL::StoreActionDontCare);
			depth->setClearDepth(1.0);
		}
	} else {
		MTL::Texture *resolve = target->resolve_attachment();
		color->setTexture(target->color_attachment());
		if (resolve != nullptr) {
			color->setResolveTexture(resolve);
			color->setStoreAction(MTL::StoreActionMultisampleResolve);
		} else {
			color->setStoreAction(MTL::StoreActionStore);
		}

		MTL::Texture *depth_texture = target->depth_attachment();
		has_depth = (depth_texture != nullptr);
		if (depth_texture != nullptr) {
			MTL::RenderPassDepthAttachmentDescriptor *depth = desc->depthAttachment();
			depth->setTexture(depth_texture);
			depth->setLoadAction(MTL::LoadActionClear);
			// Sampleable depth has to survive the pass; transient depth must
			// not, or its memoryless allocation becomes illegal.
			depth->setStoreAction(target->get_depth_mode() == PSIRenderTarget::DEPTH_SAMPLE
				? MTL::StoreActionStore
				: MTL::StoreActionDontCare);
			depth->setClearDepth(1.0);
		}

		_pass_signature = target->signature();
	}

	_encoder = _cmd->renderCommandEncoder(desc);
	desc->release();

	if (_encoder == nullptr) {
		psilog_err("Failed creating render command encoder");
		return nullptr;
	}

	// Counter-clockwise front faces, as the OpenGL defaults the engine relied
	// on. Metal defaults to clockwise.
	_encoder->setFrontFacingWinding(MTL::WindingCounterClockwise);
	_encoder->setViewport(MTL::Viewport{
		0.0, 0.0,
		static_cast<double>(viewport_size.x),
		static_cast<double>(viewport_size.y),
		0.0, 1.0
	});

	// Pass-level state. These used to be renderer-wide fields applied once per
	// frame, which is why a scene could not mix culled and unculled objects.
	switch (pass.get_cull_mode()) {
	case PSIRenderPass::CULL_FRONT:
		_encoder->setCullMode(MTL::CullModeFront);
		break;
	case PSIRenderPass::CULL_BACK:
		_encoder->setCullMode(MTL::CullModeBack);
		break;
	case PSIRenderPass::CULL_NONE:
	default:
		_encoder->setCullMode(MTL::CullModeNone);
		break;
	}

	// Recorded as the pass default, so an object that draws itself as
	// wireframe restores to this rather than to solid.
	set_pass_fill_mode(pass.get_fill_mode() == PSIRenderPass::FILL_LINES);

	// Depth testing on by default, as PSIGLRenderer::init() did with
	// glEnable(GL_DEPTH_TEST) -- but only when the pass actually has somewhere
	// to test against. A depth-stencil state that tests or writes depth with no
	// depth attachment bound is a Metal validation error, and a full-screen
	// pass into a DEPTH_NONE target is exactly that case.
	set_pass_depth_test(has_depth);

	// A new encoder starts with no pipeline bound, so nothing may draw until a
	// shader binds one.
	_current_shader = nullptr;

	return _encoder;
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
		update_pass_signature(_msaa_texture);
	} else {
		color->setTexture(_drawable->texture());
		color->setStoreAction(MTL::StoreActionStore);
		update_pass_signature(_drawable->texture());
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
	//
	// The fill default is established by PSIGLRenderer::render() on this path,
	// because it is a renderer-wide flag there rather than a pass property.
	set_pass_depth_test(true);

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
		update_pass_signature(_offscreen_msaa);
	} else {
		color->setTexture(target);
		color->setStoreAction(MTL::StoreActionStore);
		update_pass_signature(target);
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
	set_pass_depth_test(true);
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

// Put the drawable into the mode the two opt-in flags between them ask for.
//
// Everything downstream follows from _color_format: the MSAA colour target is
// allocated with it, the drawable pass signature reports it, and PSIGLShader
// builds a pipeline variant per signature -- so no pipeline, pass or shader
// needs to know this happened.
//
// EDR wins over colour management because it already is colour managed:
// extendedLinearDisplayP3 is linear by definition, so asking for both is not a
// conflict, it is a redundancy.
bool PSIMetalContext::apply_output_mode() {
	if (_layer == nullptr) {
		return false;
	}

	int mode = PSIMetal::LAYER_OUTPUT_LEGACY;
	MTL::PixelFormat format = MTL::PixelFormatBGRA8Unorm;

	if (_edr_output) {
		mode = PSIMetal::LAYER_OUTPUT_EDR;
		format = MTL::PixelFormatRGBA16Float;
	} else if (_color_managed) {
		mode = PSIMetal::LAYER_OUTPUT_SRGB;
		format = MTL::PixelFormatBGRA8Unorm_sRGB;
	}

	if (format == _color_format) {
		return true;
	}

	if (!PSIMetal::set_layer_output(_layer, mode)) {
		psilog_err("Could not put the drawable into output mode %d", mode);
		return false;
	}

	_color_format = format;

	// Rebuild the drawable's companion targets in the new format, through the
	// function that owns them. Releasing _msaa_texture by hand instead left
	// nothing to recreate it -- it is only built here and on resize -- so the
	// drawable pass silently lost its multisampled attachment while the
	// pipelines were still compiled for one, and validation caught the sample
	// count mismatch on the first draw.
	create_depth_texture(_drawable_size);

	if (_capture_texture != nullptr) {
		_capture_texture->release();
		_capture_texture = nullptr;
		_capture_valid = false;
	}

	// The signature held between passes is the drawable's, and it just changed.
	_pass_signature.color_format = _color_format;

	static const char *names[] = { "BGRA8Unorm (legacy, unmanaged)",
	                               "BGRA8Unorm_sRGB (colour managed)",
	                               "RGBA16Float (EDR)" };
	psilog(PSILog::VIDEO, "Drawable is now %s", names[mode]);

	return true;
}

bool PSIMetalContext::enable_edr_output(bool enabled) {
	if (enabled == _edr_output) {
		return _edr_output;
	}

	const bool was = _edr_output;
	_edr_output = enabled;

	if (!apply_output_mode()) {
		_edr_output = was;
		psilog_err("Display does not support extended dynamic range output");
		return _edr_output;
	}

	if (enabled) {
		PSIMetal::log_edr_displays(
			[](const char *name, bool capable, double headroom, bool is_current) {
				psilog(PSILog::VIDEO, "  display '%s'%s: EDR %s, headroom now %.2fx",
				       name, is_current ? " (window is here)" : "",
				       capable ? "capable" : "not available", headroom);
			},
			_window);
	}

	return _edr_output;
}

bool PSIMetalContext::enable_color_managed(bool enabled) {
	if (enabled == _color_managed) {
		return _color_managed;
	}

	const bool was = _color_managed;
	_color_managed = enabled;

	if (!apply_output_mode()) {
		_color_managed = was;
	}

	return _color_managed;
}

double PSIMetalContext::edr_headroom() const {
	return PSIMetal::layer_edr_headroom(_window);
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

	MTL::Region region = MTL::Region::Make2D(0, 0, (NS::UInteger)w, (NS::UInteger)h);
	const size_t pixels = (size_t)w * h;
	rgb->resize(pixels * 3);

	if (_edr_output) {
		// RGBA16Float, and the values run past 1.0 -- that is the point of the
		// mode. A PNG cannot hold them, so the screenshot is the SDR view of an
		// HDR frame: clamped at SDR white, which is what a camera pointed at the
		// screen would also give you for anything in the headroom.
		std::vector<_Float16> half(pixels * 4);
		_capture_texture->getBytes(half.data(), (NS::UInteger)w * 8, region, 0);

		for (size_t i = 0; i < pixels; i++) {
			for (int c = 0; c < 3; c++) {
				float v = (float)half[i * 4 + c];
				v = (v < 0.0f) ? 0.0f : (v > 1.0f ? 1.0f : v);
				(*rgb)[i * 3 + c] = (uint8_t)(v * 255.0f + 0.5f);
			}
		}
	} else {
		std::vector<uint8_t> bgra(pixels * 4);
		_capture_texture->getBytes(bgra.data(), (NS::UInteger)w * 4, region, 0);

		// The drawable is BGRA; the image writers want tightly packed RGB.
		for (size_t i = 0; i < pixels; i++) {
			(*rgb)[i * 3 + 0] = bgra[i * 4 + 2];
			(*rgb)[i * 3 + 1] = bgra[i * 4 + 1];
			(*rgb)[i * 3 + 2] = bgra[i * 4 + 0];
		}
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
