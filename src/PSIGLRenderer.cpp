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

bool write_image(const char *filepath, ImageFormat format, const MetalContextSharedPtr &metal_ctx) {
	if (metal_ctx == nullptr) {
		return false;
	}

	// Read back the frame PSIMetalContext copied aside during present(). The GL
	// version read the front buffer here; Metal drawables cannot be read after
	// presentation, so the copy is made while the frame is still being built.
	std::vector<uint8_t> buffer;
	glm::ivec2 size;

 #ifdef PROFILE_SAVE_IMAGE
	auto start = high_resolution_clock::now();
#endif
	if (!metal_ctx->read_last_frame(&buffer, &size)) {
		return false;
	}

	const GLint width = size.x;
	const GLint height = size.y;

 #ifdef PROFILE_SAVE_IMAGE
	auto stop = high_resolution_clock::now();
	auto duration = duration_cast<microseconds>(stop - start);
	plog_s("frame readback took %d us", duration.count());
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

	// Copying the drawable aside is off until something asks for it -- it is a
	// full-screen blit every frame and it forces the layer out of
	// framebuffer-only mode, so it is not free enough to leave running for a
	// feature no script uses at runtime.
	//
	// Arming applies to drawables vended from now on, so the frame this is first
	// called on cannot be read; report the miss and succeed from the next call.
	// Every caller in the tree sits in the frame loop, so a sequence export just
	// starts one frame later. Scripts that want frame 0 can arm it up front with
	// psi.renderer:set_frame_capture(true).
	if (_metal_ctx != nullptr && !_metal_ctx->is_capture_armed()) {
		_metal_ctx->arm_capture();
		return false;
	}

	if (format == ImageFormat::PNG) {
		file_ext = ".png";
	} else if (format == ImageFormat::QOI) {
		file_ext = ".qoi";
	}

	path = path_basename + file_ext;

	bool retval = write_image(path.c_str(), (ImageFormat)format, _metal_ctx);
	if (retval == true) {
		psilog(PSILog::EXPORT, "Wrote screen frame to %s, format = %s", path.c_str(), file_ext.c_str());
	}

	return retval;
}

void PSIGLRenderer::setup_lights(const ShaderSharedPtr &shader, const RenderContextSharedPtr &ctx) {
	const PSIGLShader::hot_uniforms &hot = shader->hot();

	for (const auto &light : ctx->lights) {
		PSILight::LightType type = light->get_type();
		// We don't need the opacity for the light color.
		glm::vec3 light_color = glm::vec3(light->get_color());

		switch (type) {
			case PSILight::LightType::AMBIENT:
				// Usually we only have one ambient light, so just override.
				shader->set_uniform(hot.ambient_color, light_color);
				shader->set_uniform(hot.ambient_intensity, light->get_intensity());
				break;

			case PSILight::LightType::DIRECTIONAL: {
				shader->set_uniform(hot.light_pos, light->get_pos());
				shader->set_uniform(hot.light_color, light_color);
				shader->set_uniform(hot.light_intensity, light->get_intensity());
				shader->set_uniform(hot.light_dir, light->get_dir());
				break;
			}

			case PSILight::LightType::POINT: {
				break;
			}
		}
	}
}

void PSIGLRenderer::extract_frustum_planes(const glm::mat4 &vp) {
	// Gribb-Hartmann: each clip-space bound gives a plane as a combination of
	// the rows of the view-projection matrix. glm is column-major, so row i is
	// (vp[0][i], vp[1][i], vp[2][i], vp[3][i]).
	const glm::vec4 row0(vp[0][0], vp[1][0], vp[2][0], vp[3][0]);
	const glm::vec4 row1(vp[0][1], vp[1][1], vp[2][1], vp[3][1]);
	const glm::vec4 row2(vp[0][2], vp[1][2], vp[2][2], vp[3][2]);
	const glm::vec4 row3(vp[0][3], vp[1][3], vp[2][3], vp[3][3]);

	_frustum_planes[0] = row3 + row0;   // left:   x > -w
	_frustum_planes[1] = row3 - row0;   // right:  x <  w
	_frustum_planes[2] = row3 + row1;   // bottom: y > -w
	_frustum_planes[3] = row3 - row1;   // top:    y <  w
	// Near is row2 alone rather than row3 + row2 because PSIOpenGL.h defines
	// GLM_FORCE_DEPTH_ZERO_TO_ONE -- clip space z runs 0..w here, as Metal
	// wants, not -w..w.
	_frustum_planes[4] = row2;          // near:   z > 0
	_frustum_planes[5] = row3 - row2;   // far:    z <  w

	// Normalise, so the plane distance below is a real distance and can be
	// compared against the box's projected radius.
	for (int i = 0; i < 6; i++) {
		float len = glm::length(glm::vec3(_frustum_planes[i]));
		if (len > 0.0f) {
			_frustum_planes[i] /= len;
		}
	}
}

bool PSIGLRenderer::is_inside_frustum(PSIRenderObj *obj, const RenderContextSharedPtr &ctx) const {
	// Everything below is a case where the CPU-side box is not where the GPU
	// draws the object, so it must never be culled. Erring towards drawing is
	// always safe; erring the other way makes objects vanish.
	if (!obj->is_cullable()) {
		return true;
	}
	PSIAABB &aabb = obj->get_aabb();
	if (!aabb.is_valid()) {
		// No real geometry bounds; see PSIAABB::is_valid().
		return true;
	}
	if (!obj->is_translated_by_camera() || !obj->is_depth_tested()) {
		// Skybox and screen-space elements: drawn through a different view
		// matrix than the frustum was built from.
		return true;
	}
	if (obj->has_children()) {
		// Children carry their own transforms, which this box does not cover.
		return true;
	}
	const GLMeshSharedPtr &mesh = obj->get_gl_mesh_ref();
	if (mesh == nullptr || mesh->is_instanced()) {
		// The bounds describe the base mesh, not where the instances landed.
		return true;
	}

	// Same composition order as calc_model_view_projection().
	const glm::mat4 model = obj->get_transform().get_model() * ctx->model.top();

	// Transform the box by centre and extent rather than by its eight corners:
	// the centre goes through the matrix, and the extent through the matrix's
	// absolute value, which gives the tightest axis-aligned box containing the
	// rotated one.
	const glm::vec3 center = glm::vec3(model * glm::vec4(aabb.get_center(), 1.0f));
	const glm::vec3 local_extent = aabb.get_extent();
	const glm::mat3 rs = glm::mat3(model);
	const glm::vec3 extent(
		glm::abs(rs[0][0]) * local_extent.x + glm::abs(rs[1][0]) * local_extent.y + glm::abs(rs[2][0]) * local_extent.z,
		glm::abs(rs[0][1]) * local_extent.x + glm::abs(rs[1][1]) * local_extent.y + glm::abs(rs[2][1]) * local_extent.z,
		glm::abs(rs[0][2]) * local_extent.x + glm::abs(rs[1][2]) * local_extent.y + glm::abs(rs[2][2]) * local_extent.z);

	for (int i = 0; i < 6; i++) {
		const glm::vec4 &plane = _frustum_planes[i];
		const glm::vec3 normal(plane);

		// Distance from the centre to the plane, and how far the box reaches
		// towards it. Outside only if the whole box is on the negative side.
		const float distance = glm::dot(normal, center) + plane.w;
		const float radius = glm::dot(glm::abs(normal), extent);

		if (distance + radius < 0.0f) {
			return false;
		}
	}

	return true;
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

	// One frame handle, reused every frame; see PSIFrame.
	_frame = PSIFrame::create(this);

	return 0;
}

// Render all of our renderable objects.
// TODO: rename to update_and_draw_render_objs() ?
void PSIGLRenderer::draw_render_objs(const RenderSceneSharedPtr &scene,
                                     const RenderContextSharedPtr &ctx,
                                     const CameraSharedPtr &camera) {

	// The scene's lights do not change between objects, so they are published to
	// the context once here rather than copied in per shader group.
	ctx->lights = scene->get_lights_ref();

	// Compared by address: the shader is kept alive by the object's material for
	// the whole loop, so there is nothing to own here.
	const PSIGLShader *previous_shader = nullptr;
	// Blending is part of the pipeline, so the run of objects that can share one
	// setRenderPipelineState is now keyed on the shader AND the blend choice.
	bool previous_blended = true;
	bool have_previous = false;

	for (const auto &obj : scene->m_render_objs) {
		const ShaderSharedPtr &shader = obj->get_shader_ref();
		assert(shader != nullptr);

		const bool blended = obj->wants_blending();

		// Don't change pipeline, if neither shader nor blend mode has changed.
		if (!have_previous || shader.get() != previous_shader || blended != previous_blended) {
			shader->use_program(blended);
			previous_blended = blended;
		}

		// The lights and the elapsed time are staged into the shader's uniform
		// block, not onto the encoder, so they only need rewriting when the
		// shader itself changes -- not when the same shader flips blend mode.
		if (shader.get() != previous_shader) {
			// Setup scene lightning.
			setup_lights(shader, ctx);

			// Set once per frame shader uniforms.
			shader->set_uniform(shader->hot().elapsed_time, ctx->elapsed_time);

			previous_shader = shader.get();
		}
		have_previous = true;

		// We run logic here also, so we don't have to loop the objects twice per frame.
		obj->logic(ctx);
		if (!obj->is_visible()) {
			continue;
		}
		// Cheaper than the draw it replaces, and it reuses the model matrix
		// the object has cached anyway.
		if (_frustum_culling && !is_inside_frustum(obj.get(), ctx)) {
			continue;
		}
		obj->draw(ctx);
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

	draw_scene_in_pass(scene, ctx, camera, _sorting);

	// Nothing to restore: the GL path had to switch blending and polygon mode
	// back off because they were global state. Encoder state does not outlive
	// the pass, and the next begin_frame() sets everything again.

	//psilog(PSILog::FREQ, "Scene rendered");
}

void PSIGLRenderer::draw_scene_in_pass(const RenderSceneSharedPtr &scene,
                                       const RenderContextSharedPtr &ctx,
                                       const CameraSharedPtr &camera,
                                       GLboolean sorting) {
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

			// The frustum only depends on the camera, so it is built once per
			// pass rather than per object.
			if (_frustum_culling == true) {
				extract_frustum_planes(ctx->projection.top() * ctx->view.top());
			}

			if (scene->m_render_objs.empty() != true) {
				// Sort our scene objects.
				if (sorting == true) {
					scene->sort(camera->get_pos());
				}
				// Draw render objects in the scene.
				draw_render_objs(scene, ctx, camera);
			}
		ctx->view.pop();
	ctx->projection.pop();
}

void PSIGLRenderer::end_frame() {
	if (_metal_ctx != nullptr) {
		_metal_ctx->present();
	}
	// The frame counters that drive the benchmark and capture harnesses live in
	// PSIVideo, and a pass-based script never calls its flip().
	if (_video != nullptr) {
		_video->frame_presented();
	}
}

const FrameSharedPtr &PSIGLRenderer::begin_frame() {
	// The handle is reused, so opening a frame allocates nothing. The command
	// buffer itself is opened lazily by the first pass encoded, which is what
	// lets a frame consist of offscreen passes only.
	_frame->_pass_index = 0;

	return _frame;
}

void PSIGLRenderer::encode_pass(const RenderPassSharedPtr &pass,
                                const RenderSceneSharedPtr &scene,
                                const CameraSharedPtr &camera) {
	if (_metal_ctx == nullptr || pass == nullptr) {
		return;
	}

	MTL::RenderCommandEncoder *encoder = _metal_ctx->begin_pass(*pass);
	if (encoder == nullptr) {
		// No drawable this frame (occluded), or the target failed to allocate.
		return;
	}

	if (scene != nullptr && camera != nullptr) {
		draw_scene_in_pass(scene, _ctx, camera, pass->get_sorting());
	}
}

void PSIGLRenderer::encode_fullscreen_pass(const RenderPassSharedPtr &pass,
                                           const GLMaterialSharedPtr &material) {
	if (_metal_ctx == nullptr || pass == nullptr || material == nullptr) {
		return;
	}

	MTL::RenderCommandEncoder *encoder = _metal_ctx->begin_pass(*pass);
	if (encoder == nullptr) {
		return;
	}

	const ShaderSharedPtr &shader = material->shader_ref();
	if (shader == nullptr) {
		psilog_err("Full-screen pass has a material with no shader");
		return;
	}

	// Full-screen passes are never blended: they replace the whole target.
	shader->use_program(false);
	if (!_metal_ctx->has_valid_pipeline()) {
		return;
	}

	material->bind_textures();

	// Elapsed time is the one uniform a post-processing shader is likely to
	// want; the rest of the block is about objects, and there is no object.
	shader->set_uniform(shader->hot().elapsed_time, _ctx->elapsed_time);
	shader->set_uniform(shader->hot().color, material->get_color());
	shader->bind_uniforms();

	// Three vertices, no vertex buffers and no index buffer: the vertex shader
	// builds a triangle that covers the target from vertex_id alone. A quad
	// would need geometry, and covering the screen with one triangle avoids the
	// diagonal seam two would create.
	encoder->drawPrimitives(MTL::PrimitiveTypeTriangle,
	                        (NS::UInteger)0, (NS::UInteger)3);
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
