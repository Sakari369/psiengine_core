
//
// Creating windows and handling fullscreen video.

#pragma once

#include <stdlib.h>
#include <sstream>
#include <string>

#include "PSIOpenGL.h"
#include "PSIGLUtils.h"
#include "PSIGlobals.h"
#include "PSIMath.h"

class PSIVideo {
	public:
		struct dimensions {
			// Our OpenGL context size.
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

		// Resizes the OpenGL viewport.
		void resize_viewport(GLsizei width, GLsizei height);
		// Print available and relevant to us opengl extensions.
		void print_opengl_extensions();
		// Get OpenGL version and profile string.
		std::string get_opengl_version_str();

		// Called on window resize and refresh.
		void resize_refresh() {
			glfwSwapBuffers(_window);
		}

		GLfloat get_viewport_aspect_ratio() {
			return _viewport.aspect_ratio;
		}

		void set_window_size(GLint width, GLint height) {
			_win_size.w = width;
			_win_size.h = height;
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
		GLint get_msaa_samples() {
			return _msaa_samples;
		}

		void set_fullscreen(bool fullscreen) {
			_fullscreen = fullscreen;
		}
		bool is_fullscreen() {
			return _fullscreen;
		}

		GLuint get_window_width() {
			return _viewport.size.w;
		}
		GLuint get_window_height() {
			return _viewport.size.h;
		}

		void set_vsync(bool vsync) {
			_vsync = vsync;
		}

		dimensions get_viewport_dimensions() {
			return _viewport;
		}

		glm::ivec2 get_window_size() {
			return glm::ivec2(_viewport.size.w, _viewport.size.h);
		}

		void flip() {
			glfwSwapBuffers(_window);
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

		void set_window_focus(GLint focused) {
		}

		void set_cursor_visible(bool visible) {
			GLint enabled = (visible == true) ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED;
			glfwSetInputMode(_window, GLFW_CURSOR, enabled);
		}
		bool is_cursor_visible() {
			GLint mode = glfwGetInputMode(_window, GLFW_CURSOR);
			return mode == GLFW_CURSOR_NORMAL ? true : false;
		}

	private:
		// Window object.
		GLFWwindow *_window;
		// Viewport dimensions.
		dimensions _viewport;
		// Window size. Might be different than the viewport dimensions, eg. on retina screens.
		PSIMath::size<GLint> _win_size;

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

		// Called on GLFW error.
		static void error_callback(int error, const char *desc) {
			printf("GLFW error %d: %s", error, desc);
		}

		// Get the monitor we are displaying on.
		GLFWmonitor *get_fullscreen_monitor();

		// Initialize opengl extensions loader.
		bool init_glew();

		// Set OpenGL window hints, like version and MSAA.
		void set_opengl_window_hints();

		// Print currently used MSAA samples.
		void print_msaa_samples();
		// Print viewport dimensions.
		void print_viewport_dimensions();
};
