#include "PSIGLRenderer.h"

void PSIGLRenderer::shutdown() {
	glDeleteFramebuffers(1, &_ctx->main_fbo);
	glDeleteFramebuffers(1, &_ctx->msaa_fbo);
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
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

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

	STACK_PUSH(ctx->projection);
		// Get the default projection matrix.
		ctx->projection.top() = camera->get_projection_matrix();

		STACK_PUSH(ctx->view);
			// Look at where the camera view is looking at.
			// This can be disabled per object with obj->set_is_camera_translated().
			ctx->view.top() = ctx->view.top() * camera->get_looking_at_matrix();
			// Store the camera in our context.
			// This way the objects have access to it via the context.
			ctx->camera_view = camera;

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
