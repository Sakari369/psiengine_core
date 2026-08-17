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
		// box filtering it down adds a clean NxN filter on top of MSAA -- 2x
		// supersampling plus 4x MSAA gives noticeably smoother edges than
		// either alone.
		//
		// The minification used to be Core Animation's: the layer's drawable was
		// created at the supersampled size and kCAFilterLinear brought it back
		// down on composite. That is what made fullscreen slow. macOS charges
		// several milliseconds a frame to present a drawable larger than the
		// display when the window is fullscreen, and nothing at all for the same
		// drawable in a window -- measured at a flat ~6 ms on a 7680x4320
		// drawable, independent of how much work the frame actually did. Every
		// script was capped near 40 fps fullscreen because of it, including ones
		// spending under a millisecond on the GPU.
		//
		// So the drawable is now always the size of the window and the box
		// filter is ours, encoded in present(). See _scene_texture.
		//
		// That recovers almost all of it and not quite all. plasma_cube
		// fullscreen went from a 23.2 ms mean to 17.5, against 16.7 for the same
		// scene in a window of the same size -- so about five percent of frames
		// still miss a vblank in fullscreen. It is not this: the residue is the
		// same at every supersample factor, including 1, where the frame spends
		// 2.7 ms on the GPU and the drawable is exactly the display's size, and
		// it does not move when the drawable is acquired at the other end of the
		// frame. Interleaved windowed/fullscreen runs confirm it is real and not
		// machine state. Whatever it is, it is macOS's, and it is a tenth of the
		// size of what was here before.
		//
		// Costs N^2 fill rate and N^2 render target memory. 1 disables it.
		void set_supersample_factor(int factor);
		int get_supersample_factor() const { return _supersample; }

		// Size the renderer actually draws at (window size * supersample).
		glm::ivec2 get_render_size() const { return _render_size; }

		// Antialiasing mode.
		//
		// AA_OFF is supersampling plus MSAA and nothing else -- what the engine
		// did before TAA existed. AA_TAA adds a jittered projection, a velocity
		// buffer and a temporal resolve, which is what makes it worth dropping
		// the supersample factor to 1: four times the pixels bought spatially
		// against an unbounded number of samples accumulated over time.
		//
		// Set from PSI_AA=off|taa, or psi.video:set_aa_mode(). Must be set before
		// anything is sized from get_render_size().
		enum AAMode {
			AA_OFF = 0,
			AA_TAA = 1,
		};
		void set_aa_mode(int mode);
		int get_aa_mode() const { return _aa_mode; }
		bool taa_enabled() const { return _aa_mode == AA_TAA; }

		// Post-resolve sharpening strength; 0 with TAA off. See _taa_sharpen.
		float get_taa_sharpen() const { return _taa_sharpen; }

		// How many sub-pixel offsets the jitter cycles through before it
		// repeats. Long enough to cover the pixel well, short enough that a
		// still image settles on a stable average instead of drifting through
		// new samples forever. See advance_jitter().
		static constexpr int JITTER_PERIOD = 16;

		// This frame's sub-pixel offset, in pixels, on the render-size grid.
		//
		// Zero unless TAA is on. PSIGLRenderer adds it to the projection so each
		// frame samples a different point inside the pixel, and the resolve
		// averages them; godrays.metal needs it too, because it builds its own
		// ray from the projection's diagonal and would otherwise be the one
		// unjittered thing in a jittered frame.
		glm::vec2 jitter_pixels() const { return _jitter; }

		// Where the velocity pass renders. Handed over by PSIGLRenderer, which
		// owns the target; null when TAA is off or the pass has not run yet.
		void set_velocity_texture(MTL::Texture *tex) { _velocity_texture = tex; }

		// True once a frame has been accumulated, so the resolve knows whether
		// there is any history to blend with. Cleared by anything that
		// invalidates it: a resize, a mode change, the first frame.
		bool history_valid() const { return _history_valid; }

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

		// glDepthMask, with the test left alone. For blended objects, which have
		// to be occluded by what is in front of them but must not occlude each
		// other. A no-op while the pass has the depth test off.
		void set_depth_write_enabled(bool enabled);

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

		// The drawable's format. BGRA8Unorm normally; RGBA16Float when EDR
		// output is on. The drawable pass must match it; offscreen passes are
		// free to differ, and PSIGLShader compiles a pipeline variant per
		// signature, so flipping this reaches the pipelines on its own.
		MTL::PixelFormat color_format() const { return _color_format; }

		// Extended dynamic range output. Off by default; a script opts in
		// through psi.boot { hdr_output = true }.
		//
		// Must be called before any pass is created, because PSIRenderPass
		// warms its pipelines against the signature current at that moment.
		// Returns whether EDR is on afterwards -- false with `enabled` true
		// means the layer refused.
		bool enable_edr_output(bool enabled);
		bool edr_output() const { return _edr_output; }

		// Colour-managed output: the drawable becomes 8-bit sRGB, so the linear
		// values every shader writes are encoded on write instead of being
		// handed to the panel raw. Off by default, because the demos' colours
		// and light intensities were all tuned against the unmanaged transform
		// and each one has to be retuned as it moves across.
		//
		// Same timing rule as EDR: before any pass is created.
		bool enable_color_managed(bool enabled);
		bool color_managed() const { return _color_managed; }

		// The current display's headroom above SDR white, 1.0 if there is none.
		// Polled rather than cached: macOS moves it with brightness and thermal
		// state, and with the window's screen. See PSIMetalLayer.h.
		double edr_headroom() const;
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

		// Kept for layer_edr_headroom(), which needs the window to find the
		// screen it is currently on.
		GLFWwindow *_window = nullptr;

		// The drawable's format, and whether EDR output is on. See
		// enable_edr_output().
		MTL::PixelFormat _color_format = MTL::PixelFormatBGRA8Unorm;
		bool _edr_output = false;
		bool _color_managed = false;

		// Resolves the two flags above into one layer configuration.
		bool apply_output_mode();

		// Depth buffer, recreated whenever the drawable size changes. The layer
		// only provides colour.
		MTL::Texture *_depth_texture = nullptr;

		// Multisampled colour target, resolved into the drawable at end of pass.
		// Null when MSAA is off.
		MTL::Texture *_msaa_texture = nullptr;
		int _msaa_samples = 1;

		// Supersampling factor and the window size before it is applied.
		// _render_size is _logical_size * _supersample; _drawable_size is
		// _logical_size, because the drawable must never be bigger than the
		// display. See set_supersample_factor().
		int _supersample = 1;
		glm::ivec2 _logical_size = glm::ivec2(0, 0);
		glm::ivec2 _render_size = glm::ivec2(0, 0);

		// What a pass aimed at "the drawable" actually renders into.
		//
		// The frame is assembled here at _render_size, and present() box filters
		// it down into the real drawable at _drawable_size. Kept at
		// color_format() so pass_signature() is identical to what the drawable
		// itself reported -- otherwise every shader in the tree would build a
		// second pipeline variant for a format change nothing else can see.
		MTL::Texture *_scene_texture = nullptr;
		bool create_scene_texture(glm::ivec2 size);

		// Did anything render into _scene_texture this frame? A frame that only
		// ever touched offscreen targets has nothing to show, and presenting
		// would put the previous frame's contents back on screen.
		bool _scene_dirty = false;

		// The box filter, built straight from the metallib rather than through
		// PSIGLShader: it wants one texture and four bytes of parameters, and
		// none of the uniform reflection, pass signatures or blend variants that
		// class exists to manage.
		MTL::RenderPipelineState *_resolve_pipeline = nullptr;
		bool ensure_resolve_pipeline();
		// Runs the box filter from one texture into another. `factor` is the
		// NxN block each destination pixel averages, so 1 is a straight copy.
		bool encode_box_filter(MTL::Texture *src, MTL::Texture *dst,
		                       glm::ivec2 src_size, glm::ivec2 dst_size,
		                       int factor, float sharpen);
		void encode_resolve(MTL::Texture *source, glm::ivec2 src_size,
		                    int factor, float sharpen);

		// Temporal antialiasing.
		//
		// Two history textures, ping-ponged: the resolve reads the one the last
		// frame wrote and writes the other. RGBA16Float rather than the
		// drawable's format because an 8-bit accumulator quantises every blend
		// and the error compounds over the tens of frames a still image
		// accumulates for.
		//
		// _velocity_texture is not owned here -- PSIGLRenderer renders it into a
		// PSIRenderTarget it owns and hands the texture over each frame.
		// All of these are at _drawable_size, not _render_size.
		//
		// The temporal pass used to run at the supersampled size, which is where
		// the frame is assembled -- and that is a lot of pixels to spend on it:
		// nineteen texture reads each, over four times as many pixels at 2x. It
		// doubled plasma_cube's frame.
		//
		// So the supersample box filter runs FIRST, into _taa_input, and the
		// temporal pass works on the display-sized result. The spatial detail
		// supersampling bought is already in those pixels by then, so nothing is
		// given up for it.
		int _aa_mode = AA_OFF;
		MTL::Texture *_taa_input = nullptr;
		MTL::Texture *_history[2] = { nullptr, nullptr };
		MTL::Texture *_velocity_texture = nullptr;
		MTL::RenderPipelineState *_taa_pipeline = nullptr;
		bool _history_valid = false;
		uint32_t _jitter_index = 0;
		glm::vec2 _jitter = glm::vec2(0.0f, 0.0f);

		// How hard the frame is sharpened on its way to the drawable, to undo
		// the softening the temporal resolve leaves. Only applied with TAA on;
		// see psi_sharpen() in resolve.metal. PSI_TAA_SHARPEN overrides it.
		float _taa_sharpen = 0.0f;

		bool create_history_textures(glm::ivec2 size);
		bool ensure_taa_pipeline();
		// Resolves _scene_texture against the history into the other history
		// slot, and returns what present() should then filter to the drawable.
		MTL::Texture *encode_taa(MTL::Texture *source);
		// Picks the next Halton offset. Called once per presented frame.
		void advance_jitter();

		// Takes the frame's drawable if it does not already hold one. Where this
		// is called from is a frame-pacing decision, not a correctness one --
		// see the definition.
		void acquire_drawable();

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
		MTL::DepthStencilState *_depth_state_read_only = nullptr;
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
