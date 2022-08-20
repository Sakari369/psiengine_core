#include "PSIGLRenderer.h"

#include "ext/qoi.h"
#include "ext/fpng.h"

using namespace std::chrono;

//#define PROFILE_SAVE_IMAGE true

void PSIGLRenderer::shutdown() {
	glDeleteFramebuffers(1, &_ctx->main_fbo);
	glDeleteFramebuffers(1, &_ctx->msaa_fbo);
}

enum ImageFormat {
	PNG = 0,
	QOI = 1
};

bool write_image(const char *filepath, ImageFormat format, GLFWwindow *window) {
	GLint width;
	GLint height;
	glfwGetFramebufferSize(window, &width, &height);

	GLsizei nrChannels = 3;
	GLsizei stride = nrChannels * width;
	stride += (stride % 4) ? (4 - stride % 4) : 0;

	GLsizei bufferSize = stride * height;
	std::vector<char> buffer(bufferSize);

	glPixelStorei(GL_PACK_ALIGNMENT, 4);
	glReadBuffer(GL_FRONT);

 #ifdef PROFILE_SAVE_IMAGE
	auto start = high_resolution_clock::now();
#endif
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer.data());

 #ifdef PROFILE_SAVE_IMAGE
	auto stop = high_resolution_clock::now();
	auto duration = duration_cast<microseconds>(stop - start);
	plog_s("glReadPixels took %d us", duration.count());
#endif

 #ifdef PROFILE_SAVE_IMAGE
	start = high_resolution_clock::now();
#endif

	// Use PNG or QOI target file format ? 

	bool retval = false;

	// PNG with alpha support.
	if (format == ImageFormat::PNG) {
		retval = fpng::fpng_encode_image_to_file(filepath, buffer.data(), (unsigned int)width, (unsigned int)height, 3, 0);
	// QOI, a lossless format that compresses better and quicker than PNG.
	} else if (format == ImageFormat::QOI) {
		qoi_desc desc = {(unsigned int)width, (unsigned int)height, 3, QOI_SRGB};
		retval = qoi_write(filepath, buffer.data(), &desc) > 0 ? true : false;
	}

 #ifdef PROFILE_SAVE_IMAGE
	stop = high_resolution_clock::now();
	auto duration_ms = duration_cast<milliseconds>(stop - start);
	plog_s("write_image took %d ms", duration_ms.count());
#endif

	return retval;
}

bool PSIGLRenderer::write_screen_to_file(std::string path_basename, int format) {
	std::string path;
	std::string file_ext = "";

	if (format == ImageFormat::PNG) {
		file_ext = ".png";
	} else if (format == ImageFormat::QOI) {
		file_ext = ".qoi";
	}

	path = path_basename + file_ext;

	bool retval = write_image(path.c_str(), (ImageFormat)format, _video->get_window());
	if (retval == true) {
		psilog(PSILog::EXPORT, "Wrote screen frame to %s, format = %s", path.c_str(), file_ext.c_str());
	}

	return retval;
}

void PSIGLRenderer::setup_lights(const ShaderSharedPtr &shader, const RenderContextSharedPtr &ctx) {
	GLint light_index = 0;
	for (auto light : ctx->lights) {
		PSILight::LightType type = light->get_type();
		// We don't need the opacity for the light color.
		glm::vec3 light_color = glm::vec3(light->get_color());

		switch (type) {
			case PSILight::LightType::AMBIENT:
				// Usually we only have one ambient light, so just override.
				shader->set_uniform("u_ambient.color", light_color);
				shader->set_uniform("u_ambient.intensity", light->get_intensity());
				break;

			case PSILight::LightType::DIRECTIONAL: {
				shader->set_uniform("u_light.pos", light->get_pos());
				shader->set_uniform("u_light.color", light_color);
				shader->set_uniform("u_light.intensity", light->get_intensity());
				shader->set_uniform("u_light.dir", light->get_dir());
				break;
			}

			case PSILight::LightType::POINT: {
				break;
			}
		}

		light_index++;
	}
}

GLint PSIGLRenderer::init_offscreen_texture(glm::ivec2 size) {
	// Generate the framebuffer object for the offscreen rendering.
	glGenFramebuffers(1, &_offscreen_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, _offscreen_fbo);

	// Generate the texture we are going to render offscreen to.
	_offscreen_texture = PSIGLTexture::create();
	_offscreen_texture->set_format(GL_RGB);

	// Set size and initialize.
	_offscreen_texture->set_size(size);
	_offscreen_texture->init();
	_offscreen_texture->set_sample_mode(PSIGLTexture::TexSampleMode::NEAREST);

	// Generate depth buffer.
	glGenRenderbuffers(1, &_offscreen_depth_buffer);
	glBindRenderbuffer(GL_RENDERBUFFER, _offscreen_depth_buffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, size.x, size.y);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _offscreen_depth_buffer);

	// Set texture as color attachment #0.
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, _offscreen_texture->get_id(), 0);

	// Set the list of draw buffers.
	GLenum draw_buffers[1] = {GL_COLOR_ATTACHMENT0};
	glDrawBuffers(1, draw_buffers); // "1" is the size of draw_buffers.

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		printf("Failed initializing offscreen framebuffer");
		return -1;
	}

	return 0;
}

GLint PSIGLRenderer::init() {
	// Create our rendering context.
	_ctx = PSIRenderContext::create();

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	if (_msaa_samples > 1) {
		glEnable(GL_MULTISAMPLE);
	}

	if (_cull_mode != CullMode::DISABLED) {
		glEnable(GL_CULL_FACE);
		GLenum face = _cull_mode == CullMode::BACK ? GL_BACK : GL_FRONT;
		glCullFace(face);
	}

	// Projection, model and view matrixes
	// Set up as identity as default.
	_ctx->projection.push(glm::mat4(1.0f));
	_ctx->model.push(glm::mat4(1.0f));
	_ctx->view.push(glm::mat4(1.0f));

	// Main and MSAA frame buffer objects.
	glGenFramebuffers(1, &_ctx->main_fbo);
	glGenFramebuffers(1, &_ctx->msaa_fbo);

	check_gl_error();

	return 0;
}

// Render all of our renderable objects.
// TODO: rename to update_and_draw_render_objs() ?
void PSIGLRenderer::draw_render_objs(const RenderSceneSharedPtr &scene,
                                     const RenderContextSharedPtr &ctx,
                                     const CameraSharedPtr &camera) {

	ShaderSharedPtr previous_shader = nullptr;
	for (const auto &obj : scene->m_render_objs) {
		ShaderSharedPtr shader = obj->get_shader();
		assert(shader != nullptr);

		// Don't change shader, if shader has not changed.
		if (shader != previous_shader) {
			shader->use_program();

			// Setup scene lightning.
			ctx->lights = scene->get_lights();
			setup_lights(shader, ctx);

			// Set once per frame shader uniforms.
			shader->set_uniform("u_elapsed_time", ctx->elapsed_time);

			previous_shader = shader;
		}
		
		// We run logic here also, so we don't have to loop the objects twice per frame.
		obj->logic(ctx);
		if (obj->is_visible()) {
			obj->draw(ctx);
		}
	}
}

void PSIGLRenderer::render(const RenderSceneSharedPtr &scene,
			   const RenderContextSharedPtr &ctx, 
			   const CameraSharedPtr &camera) {

	// Render directly to the screen.
	if (scene->get_render_to_texture() == true) {
		assert(_offscreen_fbo != -1);

		// In order for this to work, we need the viewport size.
		// In case of the offscreen texture rendering, the viewport size can be the 
		// size of the texture.
		glm::ivec2 texture_size = _offscreen_texture->get_size();
		glBindFramebuffer(GL_FRAMEBUFFER, _offscreen_fbo);
		glViewport(0, 0, texture_size.x, texture_size.y);
	} else {
		// Render to screen buffer.
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, _viewport_size.x, _viewport_size.y);
	}

	// Clear the screen.
	glClearColor(ctx->bg_color.r, ctx->bg_color.g, ctx->bg_color.b, ctx->bg_color.a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (_blending_enabled) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	if (_wireframe) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	// Store the camera in our context.
	// This way the objects have access to it via the context.
	ctx->camera = camera;

	STACK_PUSH(ctx->projection);
		// Get the default projection matrix.
		ctx->projection.top() = camera->get_projection_matrix();

		STACK_PUSH(ctx->view);
			// Look at where the camera view is looking at.
			// This can be disabled per object with obj->set_is_camera_translated().
			ctx->view.top() = ctx->view.top() * camera->get_looking_at_matrix();

			if (scene->m_render_objs.empty() != true) {
				// Sort our scene objects.
				if (_sorting == true) {
					scene->sort();
				}
				// Draw render objects in the scene.
				draw_render_objs(scene, ctx, camera);
			}
		ctx->view.pop();
	ctx->projection.pop();

	if (_wireframe == true) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
	if (_blending_enabled == true) {
		glDisable(GL_BLEND);
	}

	//psilog(PSILog::FREQ, "Scene rendered");
}

GLint PSIGLRenderer::set_draw_mode(GLint draw_mode) {
	_draw_mode = draw_mode;
	if (_draw_mode == DrawMode::SHADED ){
		_wireframe = false;
		_blending_enabled = true;
	} else if (_draw_mode == DrawMode::WIREFRAME ) {
		_wireframe = true;
		_blending_enabled = false;
	} else if (_draw_mode == DrawMode::WIREFRAME_BLENDED ) {
		_wireframe = true;
		_blending_enabled = true;
	}
	
	return _draw_mode;
}

GLint PSIGLRenderer::cycle_draw_mode() {
	GLint draw_mode = _draw_mode + 1;
	if (draw_mode > DrawMode::DrawMode_MAX) {
		draw_mode = 0;
	}

	return set_draw_mode(draw_mode);
}
