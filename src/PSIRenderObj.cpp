#include "PSIRenderObj.h"
#include "PSIMetalContext.h"

PSIRenderObj::PSIRenderObj() {
}

PSIRenderObj::PSIRenderObj(const PSIRenderObj &rhs) :  _mvp(rhs._mvp),
						      _render_asset(rhs._render_asset),
						      _geometry_data(rhs._geometry_data),
						      _children(rhs._children),
						      _depth_tested(rhs._depth_tested),
						      _camera_translated(rhs._camera_translated),
						      _visible(rhs._visible) 
						      {}

// Drawing method for drawing general render objects.
void PSIRenderObj::draw(const RenderContextSharedPtr &ctx) {
	// References throughout: every by-value shared_ptr getter in this function
	// used to be a pair of atomic refcount operations, on every object of every
	// frame. The object owns all of these for the duration of the call.
	const auto &material = _render_asset.material;
	assert(material != nullptr);

	// The velocity pass draws the whole scene with one position-only shader in
	// place of every material's own; see PSIRenderContext::shader_override.
	const bool overridden = (ctx->shader_override != nullptr);
	const auto &own_shader = material->shader_ref();
	assert(own_shader != nullptr);

	const auto &mesh_for_shader = get_gl_mesh_ref();
	const bool instanced = (mesh_for_shader != nullptr && mesh_for_shader->is_instanced());

	const ShaderSharedPtr &shader = overridden
		? (instanced && ctx->shader_override_instanced != nullptr
			? ctx->shader_override_instanced
			: ctx->shader_override)
		: own_shader;
	assert(shader != nullptr);
	const PSIGLShader::hot_uniforms &hot = shader->hot();

	// Are we rendering as wireframe ?
	//
	// ctx->wireframe used to be ORed in here. It was never bound to Lua and
	// never written from C++, so it was always false; the reachable controls
	// are this material flag and the pass's own fill mode.
	bool wireframe = (material->get_wireframe() == true);
	if (wireframe == true && PSI_G::metal_ctx != nullptr) {
		PSI_G::metal_ctx->set_fill_mode(true);
	}

	// Should this object be depth tested ?
	GLboolean disable_depth_test = !is_depth_tested();
	if (disable_depth_test == true && PSI_G::metal_ctx != nullptr) {
		PSI_G::metal_ctx->set_depth_test_enabled(false);
	}

	// Should it write depth ? Blended objects test but do not write; see the
	// note on the read-only depth state in PSIMetalContext.
	GLboolean disable_depth_write = is_depth_tested() && !is_depth_written();
	if (disable_depth_write == true && PSI_G::metal_ctx != nullptr) {
		PSI_G::metal_ctx->set_depth_write_enabled(false);
	}

	// Check if we should lock the object in place
	// Used for example for SkyMesh and UI elements
	GLboolean is_translated = is_translated_by_camera();
	if (is_translated == false) {
		STACK_PUSH(ctx->view);
		ctx->view.top() = ctx->camera->get_looking_at_matrix_without_translation();
	}

	// Get our rendering assets.
	render_asset &asset = get_render_asset();

	// Update mesh color data if material needs update.
	if (material->needs_update() == true) {
		//psilog(PSILog::OPENGL, "Updating material with color %s", GLM_CSTR(color));
		assert(_geometry_data != nullptr);
		glm::vec4 color = material->get_color();
		std::fill(_geometry_data->colors.begin(), _geometry_data->colors.end(), color);

		const auto &mesh = get_gl_mesh_ref();
		if (mesh != nullptr) {
			//psilog(PSILog::OPENGL, "Updating color data");
			//
			// Through the rotating path, not buffer_sub_data(): this runs from
			// inside draw(), and scripts that recolour every frame (psiengine)
			// would otherwise be memcpying into Shared memory the GPU is still
			// reading for the two frames in flight behind this one.
			mesh->update_color_data(&_geometry_data->colors[0],
			                        _geometry_data->colors.size() * sizeof(glm::vec4));
		}

		material->set_needs_update(false);
	}

	// Setup textures.
	//
	// There is no glActiveTexture equivalent: each texture goes onto the
	// encoder at the slot its shader declared the matching name at, resolved
	// from reflection. See PSIGLMaterial::bind_textures().
	//
	// Skipped for the velocity pass, which reads positions and nothing else --
	// binding a material's textures to a shader that declares none is only
	// wasted work.
	if (!overridden) {
		material->bind_textures(shader);
	}

	// Calculate mvp matrix for the shader.
	//
	// The non-interpolated path deliberately passes the object's own transform
	// rather than a copy: the model matrix is cached on the transform, and a
	// fresh copy every frame would throw that cache away every frame.
	if (_interpolate_transform == true) {
		// Interpolate new translation between current transform and previous transform.
		PSIGLTransform render_transform = asset.transform;
		render_transform.interpolate_from(asset.p_transform, ctx->transform_interpolation);
		calc_mvp_with_history(ctx, render_transform);
	} else {
		calc_mvp_with_history(ctx, asset.transform);
	}

	// Set uniforms specific for this render object.
	shader->set_uniform(hot.mvp_matrix, get_model_view_projection_matrix());
	shader->set_uniform(hot.prev_mvp_matrix, get_prev_model_view_projection_matrix());
	shader->set_uniform(hot.jitter, ctx->jitter_ndc);

	// The unpremultiplied matrices, for every object rather than only instanced
	// ones.
	//
	// Instanced shaders need them because the per-instance transform has to sit
	// between this object's model matrix and the view, so a premultiplied MVP is
	// no use to them. But any shader doing world-space work needs them too --
	// reflections, for one, which need the fragment's world position and, from
	// the view matrix, where the eye is. Without these an ordinary object can
	// reach clip space and nothing else.
	//
	// They cost three matrix copies. PSIUniforms is one fixed 480-byte block
	// containing these fields whether or not they are filled, and the whole
	// block is pushed on every draw regardless, so this adds no bandwidth --
	// only the memcpy into a buffer that was already going.
	shader->set_uniform(hot.model_matrix, get_model_matrix());
	shader->set_uniform(hot.view_matrix, ctx->view.top());
	shader->set_uniform(hot.projection_matrix, ctx->projection.top());

	const auto &gl_mesh = get_gl_mesh_ref();
	if (gl_mesh != nullptr && gl_mesh->is_instanced()) {
		// Identity: the instanced vertex shader transforms the normal by the
		// combined model matrix itself, so per-instance rotation affects
		// lighting. That makes fragment_phong's `u_normal_matrix * f_normal` a
		// no-op and lets the fragment shader be shared unchanged.
		shader->set_uniform(hot.normal_matrix, glm::mat3(1.0f));

		// Push any pending instance edits before the draw reads the buffer.
		gl_mesh->upload_instances();
	} else {
		shader->set_uniform(hot.normal_matrix, get_normal_matrix());
	}

	// Draw the mesh
	draw_mesh();

	// Render children of this object, if any.
	for (const auto &child : get_children_ref()) {
		child->draw(ctx);
	}

	// Back to whatever the pass asked for -- not to a hardcoded default.
	if (disable_depth_write == true && PSI_G::metal_ctx != nullptr) {
		PSI_G::metal_ctx->restore_depth_test();
	}
	if (disable_depth_test == true && PSI_G::metal_ctx != nullptr) {
		PSI_G::metal_ctx->restore_depth_test();
	}
	if (wireframe == true && PSI_G::metal_ctx != nullptr) {
		PSI_G::metal_ctx->restore_fill_mode();
	}
	if (is_translated == false) {
		ctx->view.pop();
	}
}

void PSIRenderObj::calc_mvp_with_history(const RenderContextSharedPtr &ctx,
                                         PSIGLTransform &transform) {
	// Roll the previous frame's matrices over. Only in the velocity pass, and
	// only once per frame -- see _prev_mvp. _mvp still holds what the last
	// velocity pass computed, so this has to happen before the calc below.
	if (ctx->velocity_pass && _prev_mvp_frame != ctx->elapsed_frames) {
		if (_has_prev_mvp) {
			_prev_mvp = _mvp;
		}
		_prev_mvp_frame = ctx->elapsed_frames;
	}

	calc_model_view_projection(ctx, transform);

	// The first ever draw has no previous frame, so it reports no motion rather
	// than motion from an uninitialised matrix.
	if (ctx->velocity_pass && !_has_prev_mvp) {
		_prev_mvp = _mvp;
		_has_prev_mvp = true;
	}
}

void PSIRenderObj::calc_model_view_projection(const RenderContextSharedPtr &ctx, PSIGLTransform &transform) {
	// Calculate model, view and projection matrixes. Multiplication order matters.
	_mvp.model                      = transform.get_model() * ctx->model.top();
	_mvp.model_view                 = ctx->view.top() * _mvp.model;
	_mvp.projection                 = ctx->projection.top();
	_mvp.model_view_projection      = _mvp.projection * _mvp.model_view;
}

void PSIRenderObj::init_buffers(const GLMeshSharedPtr &mesh, const GeometryDataSharedPtr &geometry_data) {
	for (const auto &buffer : geometry_data->buffers) {
		mesh->bind_buffer(buffer.target, buffer.name_id);

		const GLvoid *data_ptr;
		switch (buffer.name_id) {
		case PSIGLMesh::BufferName::POSITION:
			data_ptr = &geometry_data->positions[0];
			break;
		case PSIGLMesh::BufferName::COLOR:
			data_ptr = &geometry_data->colors[0];
			break;
		case PSIGLMesh::BufferName::NORMAL:
			data_ptr = &geometry_data->normals[0];
			break;
		case PSIGLMesh::BufferName::TEXCOORD:
			data_ptr = &geometry_data->texcoords[0];
			break;
		case PSIGLMesh::BufferName::INDEX:
			data_ptr = &geometry_data->indexes[0];
			break;
		}

		mesh->buffer_data(buffer.target, buffer.size, data_ptr, buffer.usage);

		psilog(PSILog::OPENGL, "Initialized buffer with name_id %d, id= %d, target = %d, size = %d", 
					buffer.name_id, mesh->get_buffer_id(buffer.name_id), buffer.target, buffer.size);
	}

	GLuint shader_prog = get_shader()->get_program();
	for (const auto &attrib : geometry_data->attributes) {
		mesh->bind_buffer(GL_ARRAY_BUFFER, attrib.buffer_name_id);
		mesh->enable_vertex_attrib(shader_prog, attrib.name, attrib.size, attrib.stride, attrib.pointer, attrib.type);

		psilog(PSILog::OPENGL, "Attribute '%s' added to program %d (size = %d stride = %d type = %d)",
					attrib.name, shader_prog, attrib.size, attrib.stride, attrib.type);
	}
}

void PSIRenderObj::update_aabb_from_geometry() {
	if (_geometry_data == nullptr || _geometry_data->positions.empty()) {
		return;
	}

	// One pass at load time. Until now nothing ever wrote these, so every
	// object claimed to be a unit cube -- see PSIAABB::is_valid().
	glm::vec3 min = _geometry_data->positions[0];
	glm::vec3 max = min;

	for (const glm::vec3 &p : _geometry_data->positions) {
		min = glm::min(min, p);
		max = glm::max(max, p);
	}

	_render_asset.aabb.set_bounds(min, max);
}

GLMeshSharedPtr PSIRenderObj::create_gl_mesh(const GeometryDataSharedPtr &gpu_data) {
	assert(gpu_data != nullptr);
	assert(gpu_data->positions.size() > 0);
	assert(get_material() != nullptr);
	assert(get_shader() != nullptr);

	// PSITextRenderer builds its geometry and hands it straight to this call
	// without going through set_geometry_data(), so the bounds are taken here
	// too rather than only there.
	if (_geometry_data == nullptr) {
		set_geometry_data(gpu_data);
	} else {
		update_aabb_from_geometry();
	}

	// Create a new mesh.
	auto mesh = PSIGLMesh::create();
	if (mesh->init() == false) {
		psilog_err("Failed creating GL mesh!");
		return nullptr;
	}

	mesh->bind_vao();

	init_buffers(mesh, gpu_data);

	mesh->set_draw_mode(GL_TRIANGLES);
	mesh->set_draw_count(gpu_data->indexes.size());

	psilog(PSILog::OPENGL, "Created mesh, positions.size() = %d, draw_count = %d", 
				gpu_data->positions.size(), mesh->get_draw_count());

	return mesh;
}
