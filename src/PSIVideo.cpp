#include "PSIVideo.h"

const GLint PSIVideo::DEF_SCREEN_WIDTH = 1280;
const GLint PSIVideo::DEF_SCREEN_HEIGHT = 720; 
const GLint PSIVideo::DEF_MSAA_SAMPLES = 8;

PSIVideo::~PSIVideo() {
}

void PSIVideo::shutdown() {
	// Tear the Metal context down first: it waits for in-flight frames, which
	// still reference the layer owned by the window.
	if (_metal_ctx != nullptr) {
		_metal_ctx->shutdown();
		_metal_ctx = nullptr;
	}
	if (_window != nullptr) {
		glfwDestroyWindow(_window);
		_window = nullptr;
	}
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
	GLFWmonitor *fullscreen_monitor = get_fullscreen_monitor();

	// Get content scaling.
	glfwGetMonitorContentScale(fullscreen_monitor, &_content_scaling.x, &_content_scaling.y);
	psilog(PSILog::VIDEO, "monitor content scale = %f x %f", _content_scaling.x, _content_scaling.y);

	// Get resolution for desired monitor.
	const GLFWvidmode *fullscreen_mode = glfwGetVideoMode(fullscreen_monitor);

	if (!is_fullscreen()) {
		// If we are not going fullscreen, set the monitor to nullptr in order to create a window.
		fullscreen_monitor = nullptr;
	} else {
		glfwWindowHint(GLFW_RED_BITS, fullscreen_mode->redBits);
		glfwWindowHint(GLFW_GREEN_BITS, fullscreen_mode->greenBits);
		glfwWindowHint(GLFW_BLUE_BITS, fullscreen_mode->blueBits);
		glfwWindowHint(GLFW_REFRESH_RATE, fullscreen_mode->refreshRate);

		// Use the window size we got from the fullscreen monitor.
		_win_size.x = _content_scaling.x * fullscreen_mode->width;
		_win_size.y = _content_scaling.y * fullscreen_mode->height;

		psilog(PSILog::VIDEO, "Window size got from fullscreen mode = %d x %d", _win_size.x, _win_size.y);
	}

	glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);

	_window = glfwCreateWindow(_win_size.x, _win_size.y, _window_title.c_str(), fullscreen_monitor, NULL);
	if (_window == nullptr) {
		psilog_err("Failed creating window");
		return false;
	}

	// Disable cursor if fullscreen.
	if (is_fullscreen()) {
		set_cursor_visible(false);
	}

	// Get the actual drawable size.
	// This might be different from the viewport On high-DPI displays (eg. retina).
	GLint framebuf_width;
	GLint framebuf_height;
	glfwGetFramebufferSize(_window, &framebuf_width, &framebuf_height);

	psilog(PSILog::VIDEO, "win_width = %d win_height = %d framebuf_width = %d framebuf_height = %d", _win_size.x, _win_size.y, framebuf_width, framebuf_height);

	glm::ivec2 viewport_size;
	if (is_fullscreen()) {
		// In fullscreen the monitor scaling affects the framebuffer width and height.
		viewport_size.x = _win_size.x;
		viewport_size.y = _win_size.y;
	} else {
		viewport_size.x = framebuf_width;
		viewport_size.y = framebuf_height;
	}

	// Create the Metal device, queue and swapchain layer before the first
	// resize_viewport() so the drawable gets sized along with the viewport.
	_metal_ctx = PSIMetalContext::create();
	if (!_metal_ctx->init(_window, viewport_size, _content_scaling.x)) {
		psilog_err("Failed initializing Metal context");
		return false;
	}

	// Publish the context: PSIGLShader/Mesh/Texture are constructed from Lua and
	// have no other way to reach the device or the active encoder.
	PSI_G::metal_ctx = _metal_ctx.get();

	// Must happen before any shader compiles: every render pipeline has to
	// declare the same sample count as the render pass.
	_metal_ctx->set_msaa_samples(_msaa_samples);
	// Report back what the device actually supported.
	_msaa_samples = _metal_ctx->get_msaa_samples();

	_metal_ctx->set_vsync(_vsync);
	if (_vsync) {
		psilog(PSILog::VIDEO, "Enabled VSYNC");
	} else {
		psilog(PSILog::VIDEO, "Disabled VSYNC");
	}

	// Resize our drawable to the actual frame buffer size.
	resize_viewport(viewport_size.x, viewport_size.y);

	// Information.
	print_msaa_samples();
	print_viewport_dimensions();

	// Show mouse cursor ?
	if (_cursor_disabled) {
		set_cursor_visible(false);
	}
	glfwSetCursorPos(_window, 0, 0);

	return true;
}

void PSIVideo::set_opengl_window_hints() {
	// No client API: GLFW creates the window, Metal owns the drawing surface via
	// a CAMetalLayer attached in PSIMetalContext::init().
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	// MSAA is no longer a window hint. Under Metal it means rendering into a
	// multisampled texture and resolving into the drawable, which the renderer
	// sets up. Logged here so the existing startup output does not change.
	if (_msaa_samples > 1) {
		psilog(PSILog::VIDEO, "Creating window with %d MSAA samples", _msaa_samples);
	}
}

void PSIVideo::print_viewport_dimensions() {
	// Read from our own state now; there is no glGetIntegerv(GL_VIEWPORT) to ask.
	psilog_err("viewport size = %d x %d", _viewport.size.w, _viewport.size.h);
}

void PSIVideo::print_msaa_samples() {
	// Reports the count the device actually granted, which may be lower than
	// requested -- set_msaa_samples() steps down to a supported value.
	psilog_err("MSAA: using %d sample(s)", _msaa_samples);
}

// Name kept for the Lua API: every script calls this at startup through
// psi.internal_status() (assets/scripts/psi/util.lua:16), so it must keep
// returning a descriptive string.
std::string PSIVideo::get_opengl_version_str() {
	if (_metal_ctx == nullptr) {
		return "Metal: not initialized";
	}
	return _metal_ctx->get_device_info_str();
}

// Name kept for the Lua API (psi.video:print_opengl_extensions()). The GL
// extension list and the geometry-shader output limits it used to print have no
// Metal equivalent -- geometry shaders do not exist here at all -- so this
// reports the device capabilities that actually matter now.
void PSIVideo::print_opengl_extensions() {
	if (_metal_ctx == nullptr || _metal_ctx->device() == nullptr) {
		psilog(PSILog::MSG, "No Metal device");
		return;
	}

	MTL::Device *device = _metal_ctx->device();

	psilog(PSILog::MSG, "%s", _metal_ctx->get_device_info_str().c_str());
	psilog(PSILog::MSG, "unified memory = %s", device->hasUnifiedMemory() ? "yes" : "no");
	psilog(PSILog::MSG, "max buffer length = %llu MB",
	       (unsigned long long)(device->maxBufferLength() / (1024 * 1024)));
	psilog(PSILog::MSG, "max threads per threadgroup = %lu",
	       (unsigned long)device->maxThreadsPerThreadgroup().width);

	// Closest equivalent of the old GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT: Metal
	// fixes the ceiling at 16 for MTLSamplerDescriptor::maxAnisotropy.
	psilog(PSILog::MSG, "max sampler anisotropy = 16");

	for (int samples = 2; samples <= 8; samples *= 2) {
		if (device->supportsTextureSampleCount(samples)) {
			psilog(PSILog::MSG, "supports %dx MSAA", samples);
		}
	}
}

void PSIVideo::resize_viewport(GLsizei width, GLsizei height) {
	GLfloat ratio;

	// Prevent divide by 0.
	if (height <= 0) {
		height = 1;
	}

	ratio = (GLfloat)width / (GLfloat)height;

	// Update internal representation.
	_viewport.size.w = width;
	_viewport.size.h = height;
	_viewport.aspect_ratio = ratio;

	// Resize the Metal drawable and depth buffer to match. The per-pass viewport
	// is set by PSIMetalContext::begin_frame().
	if (_metal_ctx != nullptr) {
		_metal_ctx->resize(glm::ivec2(width, height));
	}

	psilog(PSILog::VIDEO, "Viewport size changed to %dx%d, ratio=%f", width, height, ratio);
}

GLFWmonitor *PSIVideo::get_fullscreen_monitor() const {
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

glm::vec2 PSIVideo::get_monitor_content_scaling() {
	GLFWmonitor *monitor = get_fullscreen_monitor();

	GLfloat xscale;
	GLfloat yscale;
	glfwGetMonitorContentScale(monitor, &xscale, &yscale);

	psilog(PSILog::VIDEO, "monitor content scale = %f x %f", xscale, yscale);

	// Store the content scaling.
	return glm::vec2{xscale, yscale};
}
