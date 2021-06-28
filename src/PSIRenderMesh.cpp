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

	psilog(PSILog::INIT, "Initialized PSIRenderMesh");

	return true;
}

void PSIRenderMesh::generate_color_data(const GLMaterialSharedPtr &material, const GeometryDataSharedPtr &gpu_data) {
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