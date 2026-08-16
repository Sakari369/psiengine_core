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

#include <atomic>
#include <dispatch/dispatch.h>

#include "PSIGlobals.h"
#include "PSIMetal.h"

struct GLFWwindow;
class PSIGLShader;
class PSIRenderPass;

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

		// Begin a pass that renders into `target` instead of the drawable.
		//
		// The target must be BGRA8Unorm and the pass uses the same sample count
		// as the on-screen one, because Metal bakes both into every render
		// pipeline -- differing here would mean compiling a second pipeline
		// variant per shader.
		//
		// present() still ends and commits the frame; it simply has no drawable
		// to present.
		MTL::RenderCommandEncoder *begin_offscreen_frame(MTL::Texture *target,
		                                                 const glm::vec4 &clear_color);

		// Open a pass described by a PSIRenderPass, into its target or into the
		// drawable.
		//
		// Supersedes begin_frame()/begin_offscreen_frame(), which between them
		// could only express "clear into the drawable" and "clear into the one
		// offscreen texture". This honours the pass's load action, clear colour,
		// cull mode and fill mode, and can open any number of passes into one
		// command buffer -- the first opens the frame, the rest reuse it, and
		// present() closes and commits.
		//
		// Returns nullptr when there is nothing to draw into: a drawable pass
		// with the window occluded, or a target that failed to allocate.
		MTL::RenderCommandEncoder *begin_pass(const PSIRenderPass &pass);

		// Close the pass currently open without committing the frame.
		//
		// Only needed when a frame ends without presenting; begin_pass() closes
		// the previous pass itself, and present() closes the last one.
		void end_pass() { end_encoding(); }

		// Ends encoding, presents the drawable and commits. Safe to call when no
		// frame was begun -- a script that calls flip() without render() just
		// gets a no-op rather than a stall.
		void present();

		// Copy of the most recently presented frame, as tightly packed RGB8.
		//
		// present() blits each drawable into a CPU-visible texture, which is the
		// Metal equivalent of reading GL's front buffer -- the drawable itself is
		// recycled by Core Animation the moment it is presented, so it cannot be
		// read after the fact. Returns false if no frame has been presented yet.
		bool read_last_frame(std::vector<uint8_t> *rgb, glm::ivec2 *size);

		// Start copying every presented drawable aside for read_last_frame().
		//
		// The blit is a full-screen read + write of the drawable -- ~66 MB a frame
		// at 3840x2160 -- and turning it on also forces framebufferOnly off, which
		// costs lossless compression on every write to the drawable all frame. The
		// port did both unconditionally; nothing in the tree reads the result
		// unless a script asks for a screenshot, so it is armed on demand instead.
		//
		// Once armed it stays armed, so exporting a frame sequence keeps working.
		// The frame it is armed on is already encoded, so the first capture is
		// only available from the following frame -- which is why
		// write_screen_to_file() reports a miss the first time and succeeds after.
		void arm_capture();
		bool is_capture_armed() const { return _capture_armed; }

		// Mean GPU time per frame in ms, as Metal itself reports it, and the
		// number of frames that went into it.
		//
		// Wall clock between presents does not measure the renderer: with vsync
		// off, nextDrawable() still paces the CPU to the three drawables Core
		// Animation vends, so every workload from 8 objects to 9600 instances
		// reports roughly the same frame time. GPUStartTime/GPUEndTime are the
		// GPU's own timestamps for the committed buffer and are unaffected by
		// both vsync and that pacing.
		double gpu_time_mean_ms() const;
		uint32_t gpu_time_frames() const { return _gpu_frames.load(); }

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

		// Supersampling factor.
		//
		// Apple silicon caps MSAA at 4x for this colour format, which is also
		// what the OpenGL driver granted, so multisampling alone cannot be
		// pushed further. Rendering the frame at N times the window size and
		// letting the layer minify it on composite adds a clean NxN box filter
		// on top of MSAA -- 2x supersampling plus 4x MSAA gives noticeably
		// smoother edges than either alone.
		//
		// Costs N^2 fill rate and N^2 render target memory. 1 disables it.
		void set_supersample_factor(int factor);
		int get_supersample_factor() const { return _supersample; }

		// Size the renderer actually draws at (window size * supersample).
		glm::ivec2 get_render_size() const { return _drawable_size; }

		MTL::Device *device() const { return _device; }
		MTL::CommandQueue *queue() const { return _queue; }

		// Which of the MAX_FRAMES_IN_FLIGHT slots this frame owns.
		//
		// Anything the CPU rewrites while the GPU may still be reading it has to
		// rotate through these, or frame N's memcpy lands in a buffer frames N-1
		// and N-2 are still being drawn from. See PSIGLMesh's instance and colour
		// buffers.
		//
		// Advanced once per frame, in present(), so both render() calls of a
		// double-rendered frame share a slot.
		uint32_t frame_slot() const { return _frame_counter % PSIMetal::MAX_FRAMES_IN_FLIGHT; }
		uint64_t frame_counter() const { return _frame_counter; }

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

		// Pass defaults, and how an object temporarily departs from them.
		//
		// A pass establishes a fill mode and a depth-test state when it opens;
		// individual objects override them and then have to put them back. They
		// used to put them back to a hardcoded "solid" and "depth on", which is
		// wrong the moment the pass wanted anything else -- turning on global
		// wireframe in a scene that also contained one wireframe *material*
		// silently rendered every object after it solid, because that object's
		// restore reset the encoder rather than returning it to the pass.
		//
		// set_pass_* records the default and applies it; restore_* returns to it.
		void set_pass_fill_mode(bool lines);
		void set_fill_mode(bool lines);
		void restore_fill_mode() { set_fill_mode(_pass_fill_lines); }

		void set_pass_depth_test(bool enabled);
		void restore_depth_test() { set_depth_test_enabled(_pass_depth_test); }

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

		// BGRA8Unorm, fixed by CAMetalLayer. The drawable pass must match it;
		// offscreen passes are free to differ, and PSIGLShader compiles a
		// pipeline variant per signature.
		MTL::PixelFormat color_format() const { return MTL::PixelFormatBGRA8Unorm; }
		MTL::PixelFormat depth_format() const { return MTL::PixelFormatDepth32Float; }

		// The attachment formats and sample count of the pass currently open.
		//
		// PSIGLShader::use_program() reads this to pick, or build, the pipeline
		// variant matching the pass being encoded. Valid between begin_frame()
		// or begin_offscreen_frame() and present(); outside a pass it holds the
		// drawable's signature, which is the right default for warming a cache
		// at load time.
		const PSIMetal::pass_signature &pass_signature() const { return _pass_signature; }

		glm::ivec2 get_drawable_size() const { return _drawable_size; }

		// Human-readable device description, used by PSIVideo::get_opengl_version_str()
		// so psi.internal_status() keeps printing something sensible in Lua.
		std::string get_device_info_str() const;

		bool is_valid() const { return _device != nullptr; }

		// Storage mode for render targets that never outlive their pass.
		//
		// Memoryless on Apple GPUs: the attachment lives only in tile memory,
		// no DRAM is allocated and nothing is written back. Legal only while
		// every pass using it clears on load and DontCare/MultisampleResolve on
		// store. Public because PSIRenderTarget allocates its own transient
		// depth the same way.
		MTL::StorageMode transient_storage_mode() const;

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

		// Supersampling factor and the window size before it is applied.
		// _drawable_size is _logical_size * _supersample.
		int _supersample = 1;
		glm::ivec2 _logical_size = glm::ivec2(0, 0);

		// Depth (and MSAA colour) targets for offscreen passes, allocated lazily
		// to match whatever texture is being rendered into.
		MTL::Texture *_offscreen_depth = nullptr;
		MTL::Texture *_offscreen_msaa = nullptr;
		glm::ivec2 _offscreen_size = glm::ivec2(0, 0);
		bool ensure_offscreen_targets(glm::ivec2 size);

		// CPU-visible copy of the last presented frame, for screenshots.
		// Off until arm_capture(); see there for why.
		MTL::Texture *_capture_texture = nullptr;
		glm::ivec2 _capture_size = glm::ivec2(0, 0);
		bool _capture_valid = false;
		bool _capture_armed = false;
		bool ensure_capture_texture(glm::ivec2 size);

		// The command buffer that encoded the blit into _capture_texture, retained
		// so read_last_frame() can wait for it. commit() does not block, so
		// reading the texture straight after present() would otherwise race the
		// GPU and hand back a torn frame.
		MTL::CommandBuffer *_capture_cmd = nullptr;

		// Drains the autoreleased objects this frame's encoding produced --
		// nextDrawable(), commandBuffer() and renderCommandEncoder() are all
		// autoreleased, and the Lua scripts drive the frame loop themselves
		// without ever returning to an AppKit run loop.
		//
		// Opened by whichever of begin_frame()/begin_offscreen_frame() starts the
		// frame, and drained at the end of present(). A second render() in the
		// same frame must NOT open another one.
		NS::AutoreleasePool *_frame_pool = nullptr;

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

		// Signature of the pass currently open; see pass_signature().
		PSIMetal::pass_signature _pass_signature;

		// State the current pass established, that per-object overrides restore
		// to. See set_pass_fill_mode().
		bool _pass_fill_lines = false;
		bool _pass_depth_test = true;
		// Recompute it from the current targets. Called as each pass opens.
		void update_pass_signature(MTL::Texture *color_target);

		// Per-frame state, valid between begin_frame() and present().
		CA::MetalDrawable *_drawable = nullptr;
		MTL::CommandBuffer *_cmd = nullptr;
		MTL::RenderCommandEncoder *_encoder = nullptr;
		bool _frame_started = false;

		// Frames presented so far; see frame_slot().
		uint64_t _frame_counter = 0;

		// Throttles the CPU to MAX_FRAMES_IN_FLIGHT frames ahead of the GPU.
		dispatch_semaphore_t _frame_sem = nullptr;

		// GPU time accumulator; see gpu_time_mean_ms(). Written from the command
		// buffer's completion handler, which runs on a Metal-owned thread.
		std::atomic<uint64_t> _gpu_time_ns{0};
		std::atomic<uint32_t> _gpu_frames{0};

		glm::ivec2 _drawable_size = glm::ivec2(0, 0);

		bool create_depth_texture(glm::ivec2 size);
		void end_encoding();

		// Shared by begin_frame(), begin_offscreen_frame() and begin_pass():
		// take the frame slot and open the command buffer if this is the first
		// pass of the frame, otherwise close the pass already open and reuse it.
		void open_or_continue_frame();
};
