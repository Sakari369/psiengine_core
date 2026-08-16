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
	const auto &shader = material->shader_ref();
	assert(shader != nullptr);
	const PSIGLShader::hot_uniforms &hot = shader->hot();

	MTL::RenderCommandEncoder *encoder =
		(PSI_G::metal_ctx != nullptr) ? PSI_G::metal_ctx->encoder() : nullptr;

	// Are we rendering as wireframe ?
	bool wireframe = (material->get_wireframe() == true) || ctx->wireframe;
	if (wireframe == true && encoder != nullptr) {
		encoder->setTriangleFillMode(MTL::TriangleFillModeLines);
	}

	// Should this object be depth tested ?
	GLboolean disable_depth_test = !is_depth_tested();
	if (disable_depth_test == true && PSI_G::metal_ctx != nullptr) {
		PSI_G::metal_ctx->set_depth_test_enabled(false);
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

	// Setup texture.
	const auto &texture = material->texture_ref();
	if (texture != nullptr) {
		// There is no glActiveTexture equivalent: PSIGLTexture::bind() sets the
		// texture and its sampler on the encoder at slot 0 directly, matching
		// [[texture(0)]] / [[sampler(0)]] in the shaders.
		//
		// The set_uniform("u_diffuse", 0) below picked the texture unit under
		// OpenGL. It is kept because Metal reflection reports u_diffuse as a
		// texture argument and the call is recognised as a no-op, so removing it
		// would be a behaviour change for no gain.
		shader->set_uniform("u_diffuse", 0);
		texture->bind();
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
		calc_model_view_projection(ctx, render_transform);
	} else {
		calc_model_view_projection(ctx, asset.transform);
	}

	// Set uniforms specific for this render object.
	shader->set_uniform(hot.mvp_matrix, get_model_view_projection_matrix());

	const auto &gl_mesh = get_gl_mesh_ref();
	if (gl_mesh != nullptr && gl_mesh->is_instanced()) {
		// Instanced shaders build their own MVP, because the per-instance
		// transform has to sit between this object's model matrix and the view.
		// The pre-multiplied MVP above is no use to them.
		shader->set_uniform(hot.model_matrix, get_model_matrix());
		shader->set_uniform(hot.view_matrix, ctx->view.top());
		shader->set_uniform(hot.projection_matrix, ctx->projection.top());

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

	if (texture != nullptr) {
		texture->unbind();
	}
	// Enable depth test back.
	if (disable_depth_test == true && PSI_G::metal_ctx != nullptr) {
		PSI_G::metal_ctx->set_depth_test_enabled(true);
	}
	// Enable solid rendering.
	if (wireframe == true && encoder != nullptr) {
		encoder->setTriangleFillMode(MTL::TriangleFillModeFill);
	}
	if (is_translated == false) {
		ctx->view.pop();
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
