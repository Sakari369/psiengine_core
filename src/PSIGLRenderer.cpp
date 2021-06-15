#include "PSIGLRenderer.h"

#define WIREFRAME_SUPPORT true

void PSIGLRenderer::shutdown() {
	glDeleteFramebuffers(1, &_ctx->main_fbo);
	glDeleteFramebuffers(1, &_ctx->msaa_fbo);
}

GLint PSIGLRenderer::init() {
	// Create our rendering context.
	_ctx = make_shared<PSIRenderContext>();

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
void PSIGLRenderer::draw_render_objs(const shared_ptr<PSIRenderScene> &scene,
                                     const shared_ptr<PSIRenderContext> &ctx,
                                     const shared_ptr<PSICamera> &camera) {
	std::vector<shared_ptr<PSIRenderObj>> render_objs = scene->get_render_objs();
	if (render_objs.empty()) {
		return;
	}

	if (_blending_enabled) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

#ifdef WIREFRAME_SUPPORT
	if (_wireframe) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}
#endif
	//psilog(PSILog::FREQ, "Rendering scene");

	STACK_PUSH(ctx->view);
		// Look at where the camera view is looking at.
		// This can be disabled per object with obj->get_is_camera_translated().
		ctx->view.top() = ctx->view.top() * camera->get_looking_at_matrix();
		// Store the camera in our context.
		// This way the objects have access to it via the context.
		ctx->camera_view = camera;

		// Sort our scene objects.
		if (_sorting) {
			scene->sort();
		}

		// Setup lightning.
		ctx->lights = scene->get_lights();

		// Draw the objects in our scene.
		for (auto const &obj : render_objs) {
			obj->logic(ctx);
			if (obj->is_visible()) {
				obj->draw(ctx);
			}
		}
	ctx->view.pop();

	//psilog(PSILog::FREQ, "Scene rendered");

#ifdef WIREFRAME_SUPPORT
	if (_wireframe) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
#endif
	if (_blending_enabled) {
		glDisable(GL_BLEND);
	}
}

void PSIGLRenderer::render(const shared_ptr<PSIRenderScene> &scene,
			   const shared_ptr<PSIRenderContext> &ctx, 
			   const shared_ptr<PSICamera> &camera) {
	// Render directly to the screen.
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Clear the screen.
	glClearColor(ctx->bg_color.r, ctx->bg_color.g, ctx->bg_color.b, ctx->bg_color.a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	STACK_PUSH(ctx->projection);
		// Get the default projection matrix.
		ctx->projection.top() = camera->get_projection_matrix();
		// Draw our objects.
		draw_render_objs(scene, ctx, camera);
	ctx->projection.pop();
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
	if (draw_mode > DrawMode::drawMode_MAX) {
		draw_mode = 0;
	}

	return set_draw_mode(draw_mode);
}
