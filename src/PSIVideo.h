
//
// Creating windows and handling fullscreen video.

#pragma once

#include <stdlib.h>
#include <sstream>
#include <functional>
#include <string>

#include "PSIOpenGL.h"
#include "PSIGLUtils.h"
#include "PSIGlobals.h"
#include "PSIMath.h"
#include "PSIMetalContext.h"

class PSIVideo {
	public:
		struct dimensions {
			// Our drawable size.
			PSIMath::size<GLsizei> size;
			// Width to height aspect ratio.
			GLfloat aspect_ratio;
		};

		static const GLint DEF_SCREEN_WIDTH;
		static const GLint DEF_SCREEN_HEIGHT;
		static const GLint DEF_MSAA_SAMPLES;

		PSIVideo() = default;
		~PSIVideo();

		bool init();
		void shutdown();

		// Resizes the viewport and the Metal drawable.
		void resize_viewport(GLsizei width, GLsizei height);
		// Print graphics device capabilities.
		// Name kept: bound to Lua as psi.video:print_opengl_extensions().
		void print_opengl_extensions();
		// Get graphics device description.
		// Name kept: every script calls this at startup through
		// psi.internal_status() (assets/scripts/psi/util.lua:16).
		std::string get_opengl_version_str();

		// The Metal device, queue and swapchain. Shared with PSIGLRenderer so
		// render() and flip() can work on the same frame.
		MetalContextSharedPtr get_metal_context() {
			return _metal_ctx;
		}

		// Called on window resize and refresh.
		void resize_refresh() {
			flip();
		}

		GLfloat get_viewport_aspect_ratio() const {
			return _viewport.aspect_ratio;
		}

		void set_window_size(GLint width, GLint height) {
			_win_size.x = width;
			_win_size.y = height;
		}

		void set_window_title(std::string window_title) {
			_window_title = window_title;
		}
		std::string get_window_title() {
			return _window_title;
		}

		void set_msaa_samples(GLint samples) {
			_msaa_samples = samples;
		}
		GLint get_msaa_samples() const {
			return _msaa_samples;
		}

		void set_fullscreen(bool fullscreen) {
			_fullscreen = fullscreen;
		}
		bool is_fullscreen() const {
			return _fullscreen;
		}

		void set_vsync(bool vsync) {
			_vsync = vsync;
		}

		dimensions get_viewport_dimensions() {
			return _viewport;
		}

		// Ask for a different supersample factor than the default of 3.
		//
		// Safe after init -- the context re-runs its resize -- but it must
		// happen before anything is sized from get_render_size(), so psi.boot
		// does it before the script gets a chance to make a render target.
		//
		// Refused when PSI_SUPERSAMPLE was set, so a capture or a benchmark can
		// force the factor down and have that stick.
		//
		// Costs the square of the factor in fill rate: 4 is nearly twice the
		// pixels of 3, and the demos that ask for it should be ones where edge
		// quality is the point.
		void set_supersample(GLint factor) {
			if (_metal_ctx == nullptr) {
				return;
			}
			if (_supersample_pinned) {
				psilog(PSILog::VIDEO,
				       "Ignoring the script's supersample request; PSI_SUPERSAMPLE is set");
				return;
			}
			_metal_ctx->set_supersample_factor(factor);
		}

		// The size the GPU actually renders at, which is the viewport times the
		// supersample factor -- 3 by default, see init().
		//
		// This, not get_viewport_size(), is what an offscreen render target
		// standing in for the drawable must be created at. Sized from the
		// viewport it comes out at half resolution and the pass that copies it
		// back scales it up, which throws away both the supersampling and most
		// of the benefit of multisampling underneath it.
		glm::ivec2 get_render_size() const {
			return (_metal_ctx != nullptr) ? _metal_ctx->get_render_size()
			                               : get_viewport_size();
		}

		glm::ivec2 get_viewport_size() const {
			return {_viewport.size.w, _viewport.size.h};
		}

		// Presents the frame the renderer encoded. Under OpenGL this was
		// glfwSwapBuffers; with Metal the presentation is part of the command
		// buffer, so this ends encoding, presents the drawable and commits.
		//
		// The pass-based path presents through PSIFrame instead, which reaches
		// frame_presented() by its own route.
		void flip() {
			if (_metal_ctx != nullptr) {
				_metal_ctx->present();
			}
			frame_presented();
		}

		// One frame has been shown.
		//
		// Split out of flip() because the frame can now be closed from two
		// places: a script calling psi.video:flip(), or PSIFrame::present().
		// Everything that counts frames has to see both, or a script on the
		// pass API silently loses the benchmark and capture harnesses -- which
		// is exactly what happened the first time this was wired up.
		// Extended dynamic range output. Off by default; opt in from a script
		// with psi.boot { hdr_output = true }.
		//
		// Call before any render pass is created: PSIRenderPass warms its
		// pipelines against the drawable signature current at that moment, and
		// this changes it. Returns whether EDR is on afterwards, so a script can
		// tell whether the request took.
		bool set_hdr_output(bool enabled) {
			if (_metal_ctx == nullptr) {
				return false;
			}
			return _metal_ctx->enable_edr_output(enabled);
		}
		bool get_hdr_output() const {
			return _metal_ctx != nullptr && _metal_ctx->edr_output();
		}

		// Colour-managed output. Off by default; opt in with
		// psi.boot { color_managed = true }.
		//
		// Every shader writes linear light. Unmanaged, those values are handed
		// to the panel unencoded and it applies its own ~2.2 gamma, so what you
		// see is L^2.2 -- a surface lit to 0.5 displays at 0.22, MSAA resolves
		// edges too dark, and two lights at 0.5 do not add up to one at 1.0.
		// Managed, the GPU encodes on write and all of that is correct.
		//
		// Off by default because it is a look change, not a bug fix in
		// isolation: every demo's colours and light intensities were chosen
		// against the broken transform, so a script has to be retuned in the
		// same commit that turns this on.
		//
		// Same timing rule as HDR: before any pass is created.
		bool set_color_managed(bool enabled) {
			if (_metal_ctx == nullptr) {
				return false;
			}
			return _metal_ctx->enable_color_managed(enabled);
		}
		bool get_color_managed() const {
			return _metal_ctx != nullptr && _metal_ctx->color_managed();
		}

		// How much brighter than SDR white the current display can go: 1.0 with
		// no headroom, up to about 16 on an XDR panel.
		//
		// Poll it every frame. macOS moves it with screen brightness and thermal
		// state, and it changes outright when the window is dragged to another
		// display -- which is what lets a script adapt without being restarted.
		double get_edr_headroom() const {
			return (_metal_ctx != nullptr) ? _metal_ctx->edr_headroom() : 1.0;
		}

		void frame_presented() {
			// Benchmark hook. See _bench_frames.
			if (_bench_frames > 0) {
				bench_sample();
			}

			// Verification hook. See set_capture_frame_cb().
			if (_capture_frame <= 0) {
				return;
			}
			_frames_presented++;
			if (_frames_presented == 1 && _metal_ctx != nullptr) {
				// Arm now so the target frame's drawable is readable; arming
				// only affects drawables vended afterwards.
				_metal_ctx->arm_capture();
			}
			if (_frames_presented == _capture_frame) {
				if (_on_capture_frame) {
					_on_capture_frame();
				}
				// The script's loop still finishes its iteration and flips
				// again, so this must not re-fire.
				set_window_should_close();
			}
		}

		// Writes one frame to disk and closes the window, for A/B pixel diffs.
		//
		// PSI_CAPTURE_FRAME=<n> picks the frame; the callback does the writing,
		// because the image encoders live in PSIGLRenderer (which is in the
		// application, not psicore) -- same split as set_cursor_mode_changed_cb().
		// Pair it with PSI_FIXED_FRAMETIME so frame n holds the same content on
		// every run. Does nothing unless the variable is set.
		// Is a deterministic capture configured? main.cpp uses this to stop the
		// mouse reaching the camera, so a stray nudge cannot change the frame.
		bool is_capturing() const {
			return _capture_frame > 0;
		}

		void set_capture_frame_cb(std::function<void()> cb) {
			_on_capture_frame = std::move(cb);
		}

		void poll_events() {
			glfwPollEvents();
		}

		void set_window_should_close() {
			glfwSetWindowShouldClose(_window, true);
		}

		GLint should_close_window() {
			return glfwWindowShouldClose(_window);
		}

		GLFWwindow *get_window() {
			return _window;
		}

		// Cursor visibility and cursor capture are two separate intents, tracked
		// separately here and combined in apply_cursor_mode().
		//
		// GLFW exposes them as a single tri-state (NORMAL / HIDDEN / DISABLED),
		// and the previous code collapsed them into one flag -- set_cursor_visible()
		// wrote `_mouse_locked = visible`, so is_mouse_locked() reported the
		// opposite of reality and lock_mouse(true) actually released the pointer.
		// Two flags keep each call doing what its name says.

		// Hide or show the pointer.
		//
		// Hiding also requests capture. In this engine hiding the pointer has
		// always meant "fly-camera mode" -- 13 demo scripts call
		// set_cursor_visible(false) at startup and then expect mouse-look -- and
		// look is unusable without capture because a free pointer stops at the
		// screen edge. Capture is only ever applied while the window is focused,
		// so this cannot trap the mouse.
		void set_cursor_visible(bool visible) {
			_cursor_visible = visible;
			_mouse_capture_wanted = !visible;
			apply_cursor_mode();
		}

		// Explicitly request capture, giving unbounded relative movement for
		// mouse-look. Capture implies a hidden pointer.
		//
		// Like set_cursor_visible(), this is a *request*: it only takes effect
		// while the window is focused.
		void lock_mouse(bool locked) {
			_mouse_capture_wanted = locked;
			if (locked) {
				_cursor_visible = false;
			}
			apply_cursor_mode();
		}

		// Is the pointer captured right now? Reports the effective state, not the
		// request, so it is false whenever the window is unfocused.
		bool is_mouse_locked() const {
			return _mouse_capture_wanted && _window_focused;
		}

		// Release the pointer, or take it back. Bound to F1 in psi/keyb.lua.
		//
		// Without this there is no way out of a captured pointer at all: the
		// demos hide the cursor at startup, GLFW_CURSOR_DISABLED locks it to the
		// content area, and the title bar is then unreachable -- so the window
		// cannot be moved, resized, or dragged to another display for the rest
		// of the run.
		//
		// Releasing also shows the pointer, which the scripts read through
		// is_cursor_visible() to decide whether mouse-look applies, so the
		// camera stops turning while the pointer is free. That is the same
		// coupling set_cursor_visible() has, in the other direction.
		void toggle_mouse_capture() {
			set_cursor_visible(!_cursor_visible);
		}

		// Called from the GLFW window-position callback.
		//
		// A window cannot be dragged while its pointer is captured -- macOS keeps
		// warping the pointer back into the content area, and each warp moves the
		// drag anchor, so the window walks across the screen in jumps instead of
		// following the pointer. If the window moved at all while captured,
		// something outside our control is moving it; get out of the way.
		void set_window_moved() {
			if (_mouse_capture_wanted == false) {
				return;
			}

			set_cursor_visible(true);
			psilog(PSILog::VIDEO,
			       "Window moved while the pointer was captured; released it. "
			       "F1 takes it back.");
		}

		// Called from the GLFW focus callback. Releases the pointer when another
		// application takes focus and restores capture on return.
		void set_window_focus(GLint focused) {
			_window_focused = (focused != 0);
			apply_cursor_mode();
		}

		// Invoked after every cursor mode change so the input layer can drop its
		// stale cursor reference. PSIVideo lives in psicore and InputHandler in
		// the application, so main.cpp wires the two together.
		void set_cursor_mode_changed_cb(std::function<void()> cb) {
			_on_cursor_mode_changed = std::move(cb);
		}

		bool is_cursor_visible() {
			// Ask GLFW rather than the cached flag, so this stays right even if
			// the mode is changed elsewhere.
			GLint mode = glfwGetInputMode(_window, GLFW_CURSOR);
			return mode == GLFW_CURSOR_NORMAL;
		}

		// Get monitor scaling factors for current fullscreen monitor.
		glm::vec2 get_monitor_content_scaling();

	private:
		// Window object.
		GLFWwindow *_window;

		// Viewport dimensions.
		dimensions _viewport;

		// Window size.
		glm::ivec2 _win_size = { 0.0f, 0.0f };

		// Multisampling samples.
		GLint _msaa_samples = 0;

		// Create window in fullscreen ?
		bool _fullscreen = false;

		// Wait on vsync when flipping video ?
		bool _vsync = false;

		// Show cursor ?
		bool _cursor_disabled = false;

		// Was the supersample factor set from the environment? See
		// set_supersample().
		bool _supersample_pinned = false;

		// Selected monitor id.
		std::string _monitor_id;

		// Currently selected monitor index.
		GLint _monitor_index = 0;

		// Window title string.
		std::string _window_title;

		// Monitor content scaling factors, these are eg. 2.0 on HiDPI displays.
		glm::vec2 _content_scaling = { 1.0f, 1.0f };

		// Metal device, command queue and swapchain layer.
		MetalContextSharedPtr _metal_ctx;

		// Does the application want the pointer captured ? A request, not the
		// effective state -- capture is suspended while unfocused.
		bool _mouse_capture_wanted = false;

		// Is the pointer drawn ? Capture always hides it regardless.
		bool _cursor_visible = true;

		// Does our window currently have keyboard focus ?
		bool _window_focused = true;

		// Notified after every cursor mode change; see set_cursor_mode_changed_cb().
		std::function<void()> _on_cursor_mode_changed;

		// Frame capture harness; see set_capture_frame_cb(). 0 = disabled.
		GLint _capture_frame = 0;
		GLint _frames_presented = 0;
		std::function<void()> _on_capture_frame;

		// Frame time benchmark: PSI_BENCH_FRAMES=<n> measures the wall clock
		// between presents for n frames, prints mean / p95 / max and exits.
		//
		// It lives here rather than in a script because flip() is called exactly
		// once per displayed frame by all 18 of them, so every demo becomes a
		// benchmark with no edit. Most of them drive a fixed timestep and would
		// otherwise report a constant frametime no matter what the renderer does.
		//
		// Run it with -n (vsync off), or it measures the display refresh.
		GLint _bench_frames = 0;
		std::vector<double> _bench_samples;
		double _bench_last_ms = 0.0;
		void bench_sample();

		// Resolve visibility, capture and focus into GLFW's tri-state cursor mode.
		void apply_cursor_mode() {
			GLint mode;
			const char *name;

			if (!_window_focused) {
				// Another app has focus. Never hold or hide the pointer then --
				// a captured or invisible cursor over someone else's window is
				// the behaviour that made this feel like a trap.
				mode = GLFW_CURSOR_NORMAL;
				name = "NORMAL (unfocused: released)";
			} else if (_mouse_capture_wanted) {
				// DISABLED both hides and captures; there is no captured-and-visible.
				mode = GLFW_CURSOR_DISABLED;
				name = "DISABLED (hidden, captured)";
			} else if (_cursor_visible) {
				mode = GLFW_CURSOR_NORMAL;
				name = "NORMAL (visible, free)";
			} else {
				mode = GLFW_CURSOR_HIDDEN;
				name = "HIDDEN (hidden, free)";
			}

			if (mode == _cursor_mode) {
				return;
			}
			_cursor_mode = mode;

			glfwSetInputMode(_window, GLFW_CURSOR, mode);
			psilog(PSILog::VIDEO, "Cursor mode -> %s", name);

			// Entering or leaving capture makes the reported cursor position jump,
			// so let the input layer drop its reference point.
			if (_on_cursor_mode_changed) {
				_on_cursor_mode_changed();
			}
		}

		// Last mode actually pushed to GLFW, so repeated focus events are cheap
		// and do not spam the reset callback.
		GLint _cursor_mode = GLFW_CURSOR_NORMAL;

		// Called on GLFW error.
		static void error_callback(int error, const char *desc) {
			printf("GLFW error %d: %s", error, desc);
		}

		// Get current fullscreen monitor.
		GLFWmonitor *get_fullscreen_monitor() const;

		// Set window creation hints. Metal needs GLFW_NO_API.
		void set_opengl_window_hints();

		// Print currently used MSAA samples.
		void print_msaa_samples();

		// Print viewport dimensions.
		void print_viewport_dimensions();
};