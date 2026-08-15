// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Owns the Metal device, command queue and swapchain layer, and drives one
// frame's worth of command encoding.
//
// This exists because the frame is split across two classes: Lua scripts drive
// the loop themselves and call psi.renderer:render(...) then psi.video:flip().
// render() lives on PSIGLRenderer, flip() on PSIVideo. Both need to talk to the
// same drawable and command buffer, so that state lives here and both classes
// hold a reference.

#pragma once

#include <dispatch/dispatch.h>

#include "PSIGlobals.h"
#include "PSIMetal.h"

struct GLFWwindow;
class PSIGLShader;

class PSIMetalContext;
typedef shared_ptr<PSIMetalContext> MetalContextSharedPtr;

class PSIMetalContext {
	public:
		PSIMetalContext() = default;
		~PSIMetalContext();

		static MetalContextSharedPtr create() {
			return make_shared<PSIMetalContext>();
		}

		// Creates the device and queue and attaches a CAMetalLayer to the window.
		bool init(GLFWwindow *window, glm::ivec2 drawable_size, double contents_scale);
		void shutdown();

		// Starts (or restarts) this frame's render pass and returns the encoder to
		// draw into.
		//
		// Lazily acquires the drawable, so calling it twice before present() does
		// NOT acquire a second one -- psiengine.lua and triforce.lua both call
		// render() twice per flip(), which under a naive implementation would hold
		// two drawables and deadlock at three frames in flight. The second call
		// ends the previous pass and begins a fresh one that clears again, which
		// is what the GL path did when it hit glClear a second time.
		//
		// Returns nullptr if no drawable is available this frame (window occluded
		// or minimized); callers must skip drawing in that case.
		MTL::RenderCommandEncoder *begin_frame(const glm::vec4 &clear_color);

		// Ends encoding, presents the drawable and commits. Safe to call when no
		// frame was begun -- a script that calls flip() without render() just
		// gets a no-op rather than a stall.
		void present();

		// Reallocates the drawable and depth buffer. Called on framebuffer resize.
		void resize(glm::ivec2 drawable_size);

		void set_vsync(bool enabled);

		// Multisampling.
		//
		// Under OpenGL this was a window hint (GLFW_SAMPLES) and the driver
		// handled everything. Metal needs an explicit multisampled colour target
		// that resolves into the drawable, so the sample count has to be known
		// before pipelines are built -- every render pipeline must declare the
		// same count or creation fails.
		//
		// Requested counts are clamped to what the device supports.
		void set_msaa_samples(int samples);
		int get_msaa_samples() const { return _msaa_samples; }

		MTL::Device *device() const { return _device; }
		MTL::CommandQueue *queue() const { return _queue; }

		// The precompiled shader library (psishaders.metallib, built from
		// assets/shaders/*.metal and placed next to the binary). Loaded on first
		// use; every PSIGLShader looks its entry points up here.
		MTL::Library *shader_library();

		// Depth state.
		//
		// Metal's default is depth-test-always with depth WRITES DISABLED, which
		// is not OpenGL's default and silently produces flat, order-dependent
		// output. Both states are created up front and selected per object,
		// standing in for glEnable/glDisable(GL_DEPTH_TEST).
		void set_depth_test_enabled(bool enabled);

		// The shader whose pipeline is currently set on the encoder, i.e. the
		// equivalent of OpenGL's bound program.
		//
		// It is tracked here because uniform staging and the draw call happen in
		// different classes: set_uniform() writes into the shader's CPU block and
		// PSIGLMesh issues the draw, so the mesh needs a way to flush the block
		// first. Under OpenGL each glUniform* applied to the bound program
		// immediately and there was nothing to flush.
		// Pass nullptr to mean "no usable pipeline is bound".
		void set_current_shader(PSIGLShader *shader) { _current_shader = shader; }
		PSIGLShader *current_shader() const { return _current_shader; }

		// Is a valid render pipeline bound right now?
		//
		// Draw calls must check this. Metal keeps whatever pipeline was last set
		// on the encoder, so a shader that failed to build used to leave the
		// PREVIOUS shader's pipeline active and the next mesh drew through it
		// with a mismatched vertex layout -- garbage on screen instead of a
		// missing object. It is also false at the start of a render pass, before
		// anything has been bound.
		bool has_valid_pipeline() const { return _current_shader != nullptr; }
		MTL::CommandBuffer *command_buffer() const { return _cmd; }
		MTL::RenderCommandEncoder *encoder() const { return _encoder; }

		// BGRA8Unorm, fixed by CAMetalLayer. Render pipelines must match it.
		MTL::PixelFormat color_format() const { return MTL::PixelFormatBGRA8Unorm; }
		MTL::PixelFormat depth_format() const { return MTL::PixelFormatDepth32Float; }

		glm::ivec2 get_drawable_size() const { return _drawable_size; }

		// Human-readable device description, used by PSIVideo::get_opengl_version_str()
		// so psi.internal_status() keeps printing something sensible in Lua.
		std::string get_device_info_str() const;

		bool is_valid() const { return _device != nullptr; }

	private:
		MTL::Device *_device = nullptr;
		MTL::CommandQueue *_queue = nullptr;

		// CA::MetalLayer*, created in the Objective-C++ shim.
		void *_layer = nullptr;

		// Depth buffer, recreated whenever the drawable size changes. The layer
		// only provides colour.
		MTL::Texture *_depth_texture = nullptr;

		// Multisampled colour target, resolved into the drawable at end of pass.
		// Null when MSAA is off.
		MTL::Texture *_msaa_texture = nullptr;
		int _msaa_samples = 1;

		// Precompiled shader library, loaded lazily.
		MTL::Library *_shader_library = nullptr;
		bool _shader_library_tried = false;

		// GL_LESS + depth writes (the renderer's init state), and the
		// depth-test-off variant used by skyboxes and UI elements.
		MTL::DepthStencilState *_depth_state_on = nullptr;
		MTL::DepthStencilState *_depth_state_off = nullptr;
		bool create_depth_states();

		// Non-owning: the shader whose pipeline is set on the encoder.
		PSIGLShader *_current_shader = nullptr;

		// Per-frame state, valid between begin_frame() and present().
		CA::MetalDrawable *_drawable = nullptr;
		MTL::CommandBuffer *_cmd = nullptr;
		MTL::RenderCommandEncoder *_encoder = nullptr;
		bool _frame_started = false;

		// Throttles the CPU to MAX_FRAMES_IN_FLIGHT frames ahead of the GPU.
		dispatch_semaphore_t _frame_sem = nullptr;

		glm::ivec2 _drawable_size = glm::ivec2(0, 0);

		bool create_depth_texture(glm::ivec2 size);
		void end_encoding();
};
