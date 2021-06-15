#include "PSIRenderMesh.h"

GLboolean PSIRenderMesh::init() {
	// We need our GPU data
	auto gpu_data = get_geometry_data();
	auto material = get_material();

	// Update color data if we don't have it already
	if (gpu_data->colors.size() == 0) {
		generate_color_data(material, gpu_data);
		material->set_needs_update(false);
	}

	// Create our mesh
	auto mesh = create_gl_mesh(gpu_data);
	if (mesh != nullptr) {
		set_gl_mesh(mesh);
	} else {
		return false;
	}

	add_default_uniforms();

	psilog(PSILog::INIT, "Initialized PSIRenderMesh");

	return true;
}

shared_ptr<PSIRenderMesh> PSIRenderMesh::clone() {
	shared_ptr<PSIRenderMesh> clone_mesh = make_shared<PSIRenderMesh>(*this);
	// Need to re-initialize.
	clone_mesh->init();

	return clone_mesh;
}

void PSIRenderMesh::add_default_uniforms() {
	// Make sure to clear this, if we are re-initializing the class.
	uniforms->clear();
	uniforms->add_mat4("u_model_view_projection_matrix", [this]() { return get_model_view_projection_matrix(); });
	if (_has_normal_matrix == true) {
		uniforms->add_mat3("u_normal_matrix", [this]() { return get_normal_matrix(); });
	}

	psilog(PSILog::OPENGL, "Added default uniforms");
}

void PSIRenderMesh::generate_color_data(const shared_ptr<PSIGLMaterial> &material, const shared_ptr<PSIGeometryData> &gpu_data) {
	assert(gpu_data != nullptr);
	assert(material != nullptr);
	assert(gpu_data->positions.size() > 0);

	int len = gpu_data->positions.size();
	gpu_data->colors.reserve(len);

	glm::vec4 color = material->get_color();
	for (int i=0; i<len; i++) {
		gpu_data->colors.push_back(color);
	}

	// Update the gl color buffer size.
	gpu_data->buffers.at(PSIGLMesh::BufferName::COLOR).size = len * sizeof(glm::vec4);

	psilog(PSILog::OPENGL, "Generated color data based on material color, colors.size() = %d", 
		gpu_data->colors.size());
}