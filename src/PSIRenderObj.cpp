#include "PSIRenderObj.h"

PSIRenderObj::PSIRenderObj() {
	//plog_s("constructor called!");
	// Create our mapper
	uniforms = make_unique<PSIUniformMap>();
}

PSIRenderObj::PSIRenderObj(const PSIRenderObj &rhs) : uniforms(copy_unique(rhs.uniforms)), 
						      _mvp(rhs._mvp),
						      _render_asset(rhs._render_asset),
						      _geometry_data(rhs._geometry_data),
						      _children(rhs._children),
						      _depth_tested(rhs._depth_tested),
						      _camera_translated(rhs._camera_translated),
						      _visible(rhs._visible) 
						      {}

void PSIRenderObj::setup_lights(const shared_ptr<PSIGLShader> &shader,
                                const shared_ptr<PSIRenderContext> &ctx,
                                PSIRenderObj::render_asset &inst) {
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

// Drawing method for drawing general render objects.
void PSIRenderObj::draw(const shared_ptr<PSIRenderContext> &ctx) {
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
	GLboolean enable_depth_test = !is_depth_tested();
	if (enable_depth_test == true) {
		glDisable(GL_DEPTH_TEST);
	}

	// Check if we should lock the object in place
	// Used for example for SkyMesh and UI elements
	GLboolean is_translated = is_translated_by_camera();
	if (is_translated == false) {
		STACK_PUSH(ctx->view);
		ctx->view.top() = ctx->camera_view->get_looking_at_matrix_without_translation();
	}

	// Use shader program set for this asset.
	shader->use_program();

	// Get our rendering assets.
	auto asset = get_render_asset();

	// Update colors if material needs update.
	if (material->needs_update() == true) {
		glm::vec4 color = material->get_color();

		//psilog(PSILog::OPENGL, "Updating material with color %s", GLM_CSTR(color));

		assert(_geometry_data != nullptr);
		std::fill(_geometry_data->colors.begin(), _geometry_data->colors.end(), color);

		auto mesh = get_gl_mesh();
		if (mesh != nullptr) {
			//psilog(PSILog::OPENGL, "Updating color data");

			mesh->bind_vao();
			mesh->bind_buffer(GL_ARRAY_BUFFER, PSIGLMesh::BufferName::COLOR);
			mesh->buffer_sub_data(GL_ARRAY_BUFFER, 0, 
					      _geometry_data->colors.size() * sizeof(glm::vec4),
					     &_geometry_data->colors[0]);
		}

		material->set_needs_update(false);
	}

	// Setup lightning
	if (material->is_lit()) {
		// This will send the lighting data to the shaders
		// Based on the rendering asset of this model
		setup_lights(shader, ctx, asset);
	}

	// We have a modular system .. for setting certain variables run time
	if (_modules == ModulesType::MODULES_ELAPSED_TIME) {
		shader->set_uniform("u_elapsed_time", ctx->elapsed_time);
	}

	// Setup textures
	auto texture = material->get_texture();
	if (texture != nullptr) {
		// We are binding the 0 texture here.
		glActiveTexture(GL_TEXTURE0);

		// Must update that we are using the texture 0
		// For the material diffuse.
		shader->set_uniform("u_diffuse", 0);

		// Then bind the actual texture id.
		texture->bind();

		//psilog(PSILog::FREQ, "Binding texture id = %d", texture->get_id());
	}

	material->set_uniforms();

	// Interpolate new translation between current transform and previous transform
	auto render_transform = asset.transform;

	if (_interpolate_transform == true) {
		render_transform.interpolate_from(asset.p_transform, 
						  ctx->transform_interpolation);
	}

	// Calculate mvp matrix for the shader
	calc_model_view_projection(ctx, render_transform);

	// Call our parent method to set all added uniforms
	set_shader_uniforms();

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
	if (enable_depth_test == true) {
		glEnable(GL_DEPTH_TEST);
	}
	if (wireframe == true) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
	if (is_translated == false) {
		ctx->view.pop();
	}
}

void PSIRenderObj::calc_model_view_projection(const shared_ptr<PSIRenderContext> &ctx, PSIGLTransform &transform) {
	// Calculate model, view and projection matrixes.
	// Multiplication order matters.
	_mvp.model                      = transform.get_model() * ctx->model.top();
	_mvp.model_view                 = ctx->view.top() * _mvp.model;
	_mvp.projection                 = ctx->projection.top();
	_mvp.model_view_projection      = _mvp.projection * _mvp.model_view;
}

// Set the uniforms dynamically set in uniforms.
// Need to call the shader.
void PSIRenderObj::set_shader_uniforms() {
	auto shader = get_shader();
	for (auto pair : uniforms->GLfloat_map.map ) {
		shader->set_uniform(pair.first, pair.second.get_value());
	}
	for (auto pair : uniforms->GLint_map.map ) {
		shader->set_uniform(pair.first, pair.second.get_value());
	}
	for (auto pair : uniforms->GLboolean_map.map ) {
		shader->set_uniform(pair.first, pair.second.get_value());
	}
	for (auto pair : uniforms->vec3_map.map ) {
		//plog_s("%s = %s", pair.first.c_str(), GLM_CSTR(pair.second.get_value()));
		shader->set_uniform(pair.first, pair.second.get_value());
	}
	for (auto pair : uniforms->vec4_map.map ) {
		shader->set_uniform(pair.first, pair.second.get_value());
	}
	for (auto pair : uniforms->mat3_map.map ) {
		shader->set_uniform(pair.first, pair.second.get_value());
	}
	for (auto pair : uniforms->mat4_map.map ) {
		shader->set_uniform(pair.first, pair.second.get_value());
	}
}

void PSIRenderObj::init_buffers(const shared_ptr<PSIGLMesh> &mesh, const shared_ptr<PSIGeometryData> &geometry_data) {
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

shared_ptr<PSIGLMesh> PSIRenderObj::create_gl_mesh(const shared_ptr<PSIGeometryData> &gpu_data) {
	assert(gpu_data != nullptr);
	assert(gpu_data->positions.size() > 0);
	assert(get_material() != nullptr);
	assert(get_shader() != nullptr);

	// Create a new mesh.
	auto mesh = make_shared<PSIGLMesh>();
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
