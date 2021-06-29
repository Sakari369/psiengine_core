#include "PSIRenderObj.h"

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
	auto shader = get_shader();
	assert(shader != nullptr);
	auto material = get_material();
	assert(material != nullptr);

	// Are we rendering as wireframe ?
	bool wireframe = (material->get_wireframe() == true) || ctx->wireframe;
	if (wireframe == true) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	// Should this object be depth tested ?
	GLboolean disable_depth_test = !is_depth_tested();
	if (disable_depth_test == true) {
		glDisable(GL_DEPTH_TEST);
	}

	// Check if we should lock the object in place
	// Used for example for SkyMesh and UI elements
	GLboolean is_translated = is_translated_by_camera();
	if (is_translated == false) {
		STACK_PUSH(ctx->view);
		ctx->view.top() = ctx->camera->get_looking_at_matrix_without_translation();
	}

	// Get our rendering assets.
	auto asset = get_render_asset();

	// Update mesh color data if material needs update.
	if (material->needs_update() == true) {
		//psilog(PSILog::OPENGL, "Updating material with color %s", GLM_CSTR(color));
		assert(_geometry_data != nullptr);
		glm::vec4 color = material->get_color();
		std::fill(_geometry_data->colors.begin(), _geometry_data->colors.end(), color);

		auto mesh = get_gl_mesh();
		if (mesh != nullptr) {
			//psilog(PSILog::OPENGL, "Updating color data");
			mesh->bind_vao();
			mesh->bind_buffer(GL_ARRAY_BUFFER, PSIGLMesh::BufferName::COLOR);
			mesh->buffer_sub_data(GL_ARRAY_BUFFER, 0, _geometry_data->colors.size() * sizeof(glm::vec4), &_geometry_data->colors[0]);
		}

		material->set_needs_update(false);
	}

	// Setup texture.
	auto texture = material->get_texture();
	if (texture != nullptr) {
		//psilog(PSILog::FREQ, "Binding texture id = %d", texture->get_id());
		// Bind texture unit 0.
		glActiveTexture(GL_TEXTURE0);
		// Update that we are using texture 0 for the material diffuse.
		shader->set_uniform("u_diffuse", 0);
		texture->bind();
	}

	auto render_transform = asset.transform;
	if (_interpolate_transform == true) {
		// Interpolate new translation between current transform and previous transform.
		render_transform.interpolate_from(asset.p_transform, ctx->transform_interpolation);
	}

	// Calculate mvp matrix for the shader.
	calc_model_view_projection(ctx, render_transform);

	// Set uniforms specific for this render object.
	shader->set_uniform("u_model_view_projection_matrix", get_model_view_projection_matrix());
	shader->set_uniform("u_normal_matrix", get_normal_matrix());

	// Draw the mesh
	draw_mesh();

	// Render children of this object, if any.
	auto children = get_children();
	for (auto child : children) {
		child->draw(ctx);
	}

	if (texture != nullptr) {
		texture->unbind();
	}
	// Enable depth test back.
	if (disable_depth_test == true) {
		glEnable(GL_DEPTH_TEST);
	}
	// Enable solid rendering.
	if (wireframe == true) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
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
		check_gl_error();

		psilog(PSILog::OPENGL, "Initialized buffer with name_id %d, id= %d, target = %d, size = %d", 
					buffer.name_id, mesh->get_buffer_id(buffer.name_id), buffer.target, buffer.size);
	}

	GLuint shader_prog = get_shader()->get_program();
	for (const auto &attrib : geometry_data->attributes) {
		mesh->bind_buffer(GL_ARRAY_BUFFER, attrib.buffer_name_id);
		mesh->enable_vertex_attrib(shader_prog, attrib.name, attrib.size, attrib.stride, attrib.pointer, attrib.type);
		check_gl_error();

		psilog(PSILog::OPENGL, "Attribute '%s' added to program %d (size = %d stride = %d type = %d)",
					attrib.name, shader_prog, attrib.size, attrib.stride, attrib.type);
	}
}

GLMeshSharedPtr PSIRenderObj::create_gl_mesh(const GeometryDataSharedPtr &gpu_data) {
	assert(gpu_data != nullptr);
	assert(gpu_data->positions.size() > 0);
	assert(get_material() != nullptr);
	assert(get_shader() != nullptr);

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
