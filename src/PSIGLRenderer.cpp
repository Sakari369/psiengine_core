#include "PSIGLRenderer.h"

#include "ext/qoi.h"
#include "ext/fpng.h"

using namespace std::chrono;

//#define PROFILE_SAVE_IMAGE true

void PSIGLRenderer::shutdown() {
	// Metal resources are owned by PSIMetalContext, which PSIVideo tears down.
	_offscreen_texture = nullptr;
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
	// Metal has no framebuffer objects: a render pass names its attachments
	// directly, so this only has to allocate the colour target. The matching
	// depth (and MSAA) attachments are created by PSIMetalContext when the
	// offscreen pass begins, sized to this texture.
	_offscreen_texture = PSIGLTexture::create();
	if (!_offscreen_texture->create_render_target(size.x, size.y)) {
		psilog_err("Failed initializing offscreen render target");
		_offscreen_texture = nullptr;
		return -1;
	}

	_offscreen_texture->set_sample_mode(PSIGLTexture::TexSampleMode::NEAREST);

	psilog(PSILog::INIT, "Offscreen render target ready at %dx%d", size.x, size.y);

	return 0;
}

GLint PSIGLRenderer::init() {
	// Create our rendering context.
	_ctx = PSIRenderContext::create();

	// No global render state to set up here any more.
	//
	// Depth testing, culling, blending and multisampling were all global GL
	// switches flipped once at startup. In Metal depth and blending are baked
	// into state objects and pipelines, and culling lives on the encoder, so
	// they are applied per frame in render() and per shader in compile().
	// Cubemap seamless filtering is always on.
	//
	// The main_fbo/msaa_fbo framebuffer objects are gone too; they were
	// generated and deleted but never actually bound.

	// Projection, model and view matrixes
	// Set up as identity as default.
	_ctx->projection.push(glm::mat4(1.0f));
	_ctx->model.push(glm::mat4(1.0f));
	_ctx->view.push(glm::mat4(1.0f));

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

	// Begin this frame's render pass. This clears colour and depth, and lazily
	// acquires the drawable -- scripts that call render() twice before flip()
	// (psiengine.lua, triforce.lua) get a second clearing pass on the same
	// drawable rather than a second drawable.
	//
	// Returns nullptr when the window has no drawable (occluded/minimized), in
	// which case there is nothing to draw into this frame.
	if (_metal_ctx == nullptr) {
		return;
	}
	// Render into the offscreen texture, or into the window's drawable.
	MTL::RenderCommandEncoder *encoder = nullptr;
	if (scene->get_render_to_texture() == true && _offscreen_texture != nullptr) {
		encoder = _metal_ctx->begin_offscreen_frame(
			_offscreen_texture->get_metal_texture(), ctx->bg_color);
	} else {
		encoder = _metal_ctx->begin_frame(ctx->bg_color);
	}

	if (encoder == nullptr) {
		return;
	}

	// Face culling. The GL version set this once in init() because it was global
	// state; Metal state lives on the encoder, so it is applied per frame.
	switch (_cull_mode) {
	case CullMode::FRONT:
		encoder->setCullMode(MTL::CullModeFront);
		break;
	case CullMode::BACK:
		encoder->setCullMode(MTL::CullModeBack);
		break;
	case CullMode::DISABLED:
	default:
		encoder->setCullMode(MTL::CullModeNone);
		break;
	}

	// Wireframe. Direct equivalent of glPolygonMode(GL_FRONT_AND_BACK, GL_LINE).
	if (_wireframe) {
		encoder->setTriangleFillMode(MTL::TriangleFillModeLines);
	} else {
		encoder->setTriangleFillMode(MTL::TriangleFillModeFill);
	}

	// Blending is part of the pipeline state now, set once per shader in
	// PSIGLShader::compile() with the same SRC_ALPHA / ONE_MINUS_SRC_ALPHA
	// equation this used to enable per frame. Wireframe is handled by the
	// setTriangleFillMode() call above.

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

	// Nothing to restore: the GL path had to switch blending and polygon mode
	// back off because they were global state. Encoder state does not outlive
	// the pass, and the next begin_frame() sets everything again.

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
