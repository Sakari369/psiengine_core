#include "PSIVideo.h"

const GLint PSIVideo::DEF_SCREEN_WIDTH = 1280;
const GLint PSIVideo::DEF_SCREEN_HEIGHT = 720; 
const GLint PSIVideo::DEF_MSAA_SAMPLES = 8;

PSIVideo::~PSIVideo() {
}

void PSIVideo::shutdown() {
	glfwDestroyWindow(_window);
}

bool PSIVideo::init() {
	// Set error callback.
	glfwSetErrorCallback(error_callback);
	
	// Initialize GLFW.
	if( !glfwInit()) {
		psilog_err("Failed initializing windowing system!");
		return false;
	}

	// Call terminate on exit.
	atexit(glfwTerminate);

	// Setup OpenGL window hints.
	set_opengl_window_hints();

	// Create our window.
	GLFWmonitor *monitor = nullptr;
	if (is_fullscreen() == true) {
		monitor = get_fullscreen_monitor();

		// Get resolution for desired monitor.
		const GLFWvidmode *video_mode = glfwGetVideoMode(monitor);

		glfwWindowHint(GLFW_RED_BITS, video_mode->redBits);
		glfwWindowHint(GLFW_GREEN_BITS, video_mode->greenBits);
		glfwWindowHint(GLFW_BLUE_BITS, video_mode->blueBits);
		glfwWindowHint(GLFW_REFRESH_RATE, video_mode->refreshRate);
		glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);

		// Store the window size we got from this monitor.
		_win_size.w = video_mode->width;
		_win_size.h = video_mode->height;
	}

	_window = glfwCreateWindow(_win_size.w, _win_size.h, _window_title.c_str(), monitor, NULL);	
	if (_window == nullptr) {
		psilog_err("Failed creating window");
		return false;
	}

	// Set current context.
	glfwMakeContextCurrent(_window);

	// Disable cursor if fullscreen.
	if (_fullscreen == true) {
		set_cursor_visible(false);
	}
	// Enable vsync ?
	if (_vsync == true) {
		glfwSwapInterval(1);
		psilog(PSILog::VIDEO, "Enabled VSYNC");
	} else {
		psilog(PSILog::VIDEO, "Disabled VSYNC");
	}

	// Get the actual OpenGL context frame buffer size.
	// This might be different from the viewport On high-DPI displays (eg. retina).
	GLint framebuf_width;
	GLint framebuf_height;
	glfwGetFramebufferSize(_window, &framebuf_width, &framebuf_height);

	psilog(PSILog::VIDEO, "width = %d height = %d framebuf_width = %d framebuf_height = %d", 
		_win_size.w, _win_size.h, framebuf_width, framebuf_height);

	// Resize our GL context size to the actual frame buffer size.
	resize_viewport(framebuf_width, framebuf_height);

	check_gl_error();

	// Initialize GLEW.
	if (init_glew() == false) {
		return -1;
	}

	// Information.
	print_msaa_samples();
	print_viewport_dimensions();

	// Show mouse cursor ?
	if (_cursor_disabled == true) {
		set_cursor_visible(false);
	}
	glfwSetCursorPos(_window, 0, 0);

	return true;
}

bool PSIVideo::init_glew() {
	psilog(PSILog::INIT, "Initializing GLEW (ignore possible next INVALID_ENUM error)");

	glewExperimental = GL_TRUE; 
	GLenum glewError = glewInit();

	if( glewError != GLEW_OK ) {
		psilog_err("Error initializing GLEW! %s\n", glewGetErrorString(glewError) );
		return false;
	}

	// Initializing GLEW causes invalid GL_INVALID_ENUM error probably.
	// Just get it out of the way and ignore.
	check_gl_error();
	psilog(PSILog::OPENGL, "%s", get_opengl_version_str().c_str());

	return true;
}

void PSIVideo::set_opengl_window_hints() {
	// Use OpenGL 3.3 core.
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

	// TODO: we have to query this info.
	if (_msaa_samples > 1) {
		psilog(PSILog::VIDEO, "Creating window with %d MSAA samples", _msaa_samples);
		glfwWindowHint(GLFW_SAMPLES, _msaa_samples);
	}

	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
}

void PSIVideo::print_viewport_dimensions() {
	GLint viewport_dimensions[4];
	GLfloat viewport_width;
	GLfloat viewport_height;

	glGetIntegerv(GL_VIEWPORT, viewport_dimensions);
	viewport_width = viewport_dimensions[2];
	viewport_height = viewport_dimensions[3];

	psilog_err("viewport size = %.2f x %.2f", viewport_width, viewport_height);
}

void PSIVideo::print_msaa_samples() {
	GLint bufs = 1;
	GLint samples = 1;

	glGetIntegerv(GL_SAMPLE_BUFFERS, &bufs);
	glGetIntegerv(GL_SAMPLES, &samples);

	psilog_err("MSAA enabled: using %d buffer(s) with %d sample(s)", bufs, samples);
}

std::string PSIVideo::get_opengl_version_str() {
	// Get OpenGL context info.
	GLint major = glfwGetWindowAttrib(_window, GLFW_CONTEXT_VERSION_MAJOR);
	GLint minor = glfwGetWindowAttrib(_window, GLFW_CONTEXT_VERSION_MINOR);
	GLint profile = glfwGetWindowAttrib(_window, GLFW_OPENGL_PROFILE);

	std::string profile_str;
	if (major >= 3) {
		if (profile == GLFW_OPENGL_COMPAT_PROFILE) {
			profile_str = "GLFW_OPENGL_COMPAT_PROFILE";
		} else {
			profile_str = "GLFW_OPENGL_CORE_PROFILE";
		}
	}

	std::stringstream ss;
	ss << "OpenGL version " << major << "." << minor << " (" << profile_str.c_str() << ")";

	return ss.str();
}

GLFWmonitor *PSIVideo::get_fullscreen_monitor() {
	int count;
	GLFWmonitor **monitors = glfwGetMonitors(&count);
	GLFWmonitor *dest_monitor = nullptr;

	// Have already set a monitor index to use ?
	if (_monitor_index != -1 && _monitor_index < count) {
		dest_monitor = monitors[_monitor_index];
	} else {
		// By default use the first monitor.
		dest_monitor = monitors[0];
	}

	psilog(PSILog::VIDEO, "Using monitor \"%s\"", glfwGetMonitorName(dest_monitor));

	return dest_monitor;
}

void PSIVideo::print_opengl_extensions() {
	GLint num_ext = 0;
	glGetIntegerv(GL_NUM_EXTENSIONS, &num_ext);

#ifndef NO_PRINT_DEBUG
	psilog(PSILog::MSG, "%d OpenGL Extensions available", num_ext);
	while(0 < --num_ext) {
		const unsigned char *extName = glGetStringi(GL_EXTENSIONS, num_ext - 1);
		psilog(PSILog::MSG, "%s", extName);
	}
#endif

	GLint anisotropy_max = 0;
	glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &anisotropy_max);
	psilog(PSILog::MSG, "GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT = %d", anisotropy_max);

	/*
	The other limit, defined by GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS is, in layman's terms, the 
	total amount of stuff that a single GS invocation can write. 

	It is the total number of output values (a component, in GLSL terms, is a component of a vector. 
	So a float is one component; a vec3 is 3 components) that a single GS invocation can write to. 

	This is different from GL_MAX_GEOMETRY_OUTPUT_COMPONENTS 
	(the maximum allowed number of components in out variables). 
	
	The total output component is the total number of components + vertices that can be written.
	*/

	GLint max_geometry_vertices = 0;
	glGetIntegerv(GL_MAX_GEOMETRY_OUTPUT_VERTICES, &max_geometry_vertices);
	psilog(PSILog::MSG, "GL_MAX_GEOMETRY_OUTPUT_VERTICES = %d", max_geometry_vertices);

	GLint geometry_vertices_out = 0;
	glGetIntegerv(GL_GEOMETRY_VERTICES_OUT, &geometry_vertices_out);
	psilog(PSILog::MSG, "GL_GEOMETRY_VERTICES_OUT = %d", geometry_vertices_out);

	GLint max_geometry_components = 0;
	glGetIntegerv(GL_MAX_GEOMETRY_OUTPUT_COMPONENTS, &max_geometry_components);
	psilog(PSILog::MSG, "GL_MAX_GEOMETRY_OUTPUT_COMPONENTS = %d", max_geometry_components);

	GLint max_geometry_total_components = 0;
	glGetIntegerv(GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS, &max_geometry_total_components);
	psilog(PSILog::MSG, "GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS = %d", max_geometry_total_components);

	check_gl_error();
}

void PSIVideo::resize_viewport(GLsizei width, GLsizei height) {
	GLfloat ratio;

	// Prevent divide by 0.
	if (height <= 0) {
		height = 1;
	}

	// Set up viewport.
	glViewport(0, 0, width, height);
	ratio = (GLfloat)width / (GLfloat)height;

	// Update internal representation.
	_viewport.size.w = width;
	_viewport.size.h = height;
	_viewport.aspect_ratio = ratio;

	psilog(PSILog::VIDEO, "OpenGL viewport size changed to %dx%d, ratio=%f", width, height, ratio);
}
