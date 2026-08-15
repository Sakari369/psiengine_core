
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

		glm::ivec2 get_viewport_size() const {
			return {_viewport.size.w, _viewport.size.h};
		}

		// Presents the frame the renderer encoded. Under OpenGL this was
		// glfwSwapBuffers; with Metal the presentation is part of the command
		// buffer, so this ends encoding, presents the drawable and commits.
		void flip() {
			if (_metal_ctx != nullptr) {
				_metal_ctx->present();
			}
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