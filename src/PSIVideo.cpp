#include "PSIVideo.h"

const GLint PSIVideo::DEF_SCREEN_WIDTH = 1280;
const GLint PSIVideo::DEF_SCREEN_HEIGHT = 720; 
// 2, not 8.
//
// The device caps this at 4 for BGRA8 anyway, so 8 only ever meant "as much as
// you have". With the depth and MSAA colour targets memoryless they live in
// tile memory, and tile memory is the constraint: 4x MSAA needs 4 bytes of
// colour plus 4 of depth per sample, 32 bytes per pixel, which is the whole
// 32 KB budget for a 32x32 tile and forces the driver to use smaller tiles.
// 2x halves that and leaves room.
//
// Edges do not suffer much because the frame is also 2x supersampled and box
// filtered on composite (PSIMetalContext::set_supersample_factor). -a/--antialias
// still overrides this.
const GLint PSIVideo::DEF_MSAA_SAMPLES = 2;

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

	// Apple silicon caps MSAA at 4x for this format, so supersample on top of it
	// to get edges smoother than multisampling alone can manage.
	// PSI_SUPERSAMPLE=1 turns it off if the fill rate cost matters.
	int supersample = 2;
	const char *ss_env = getenv("PSI_SUPERSAMPLE");
	if (ss_env != nullptr) {
		int parsed = atoi(ss_env);
		if (parsed >= 1 && parsed <= 4) {
			supersample = parsed;
		}
	}
	_metal_ctx->set_supersample_factor(supersample);

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

	// Frame time benchmark; see PSIVideo::_bench_frames.
	const char *bench_env = getenv("PSI_BENCH_FRAMES");
	if (bench_env != nullptr) {
		_bench_frames = atoi(bench_env);
		if (_bench_frames > 0) {
			_bench_samples.reserve(_bench_frames);
			psilog(PSILog::VIDEO, "Benchmarking %d frames", _bench_frames);
			if (_vsync) {
				psilog_err("PSI_BENCH_FRAMES with vsync on measures the display, "
				           "not the renderer -- pass -n");
			}
		}
	}

	// Frame capture harness; see PSIVideo::set_capture_frame_cb().
	const char *capture_env = getenv("PSI_CAPTURE_FRAME");
	if (capture_env != nullptr) {
		_capture_frame = atoi(capture_env);
		if (_capture_frame > 0) {
			psilog(PSILog::VIDEO, "Will capture frame %d and exit", _capture_frame);
		}
	}

	return true;
}

void PSIVideo::bench_sample() {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	double now_ms = (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;

	if (_bench_last_ms > 0.0) {
		_bench_samples.push_back(now_ms - _bench_last_ms);
	}
	_bench_last_ms = now_ms;

	if ((GLint)_bench_samples.size() < _bench_frames) {
		return;
	}

	// Drop the first 10% as warm-up: shader compilation, texture upload and the
	// first few drawable acquisitions all land in the opening frames.
	size_t warmup = _bench_samples.size() / 10;
	std::vector<double> s(_bench_samples.begin() + warmup, _bench_samples.end());
	std::sort(s.begin(), s.end());

	double sum = 0.0;
	for (double v : s) {
		sum += v;
	}
	double mean = sum / (double)s.size();
	double p50 = s[s.size() / 2];
	double p95 = s[(size_t)((double)s.size() * 0.95)];
	double max = s.back();

	double gpu_ms = (_metal_ctx != nullptr) ? _metal_ctx->gpu_time_mean_ms() : 0.0;

	printf("frames=%zu  gpu=%.3f ms  wall mean=%.3f p50=%.3f p95=%.3f max=%.3f ms\n",
	       s.size(), gpu_ms, mean, p50, p95, max);
	fflush(stdout);

	set_window_should_close();
	_bench_frames = 0;
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
