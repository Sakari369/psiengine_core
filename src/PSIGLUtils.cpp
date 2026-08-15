#include "PSIGLUtils.h"

namespace PSIGLUtils {

// For debugging matrixes
void print_mat4(glm::mat4 const &matrix) {
	std::cout << std::setfill('-') << std::setw(19) << "\n";

	for (int i=0; i<4; i++) {
		for (int j=0; j<4; j++) {
			std::cout << std::setfill(' ') << std::setw(3)
				  << std::setprecision(3) << matrix[j][i] << " ";
		}
		std::cout << "\n";
	}
}

// check_error() is gone: it polled glGetError(), which has no Metal
// equivalent. Metal reports errors through NSError at resource creation and
// through the validation layers at draw time -- run with MTL_DEBUG_LAYER=1
// and MTL_SHADER_VALIDATION=1 instead.

/*
bool createFullscreenQuadInAsset(PSIRenderObj::render_asset &asset) {
	assert(asset.shader != nullptr);

	GLuint shader_prog = asset.shader->get_program();

	GLfloat x0 = -1.0f;
	GLfloat y0 = -1.0f;
	GLfloat x1 = 1.0f;
	GLfloat y1 = 1.0f;

	std::vector<glm::vec3> quad = {
		glm::vec3(x0, y0, 0.0f),
		glm::vec3(x0, y1, 0.0f),
		glm::vec3(x1, y1, 0.0f),
		glm::vec3(x1, y0, 0.0f)
	};

	assert(asset.mesh != nullptr);

	// Initialize with positions
	if (asset.mesh->init(quad, quad.size(), GL_TRIANGLES, GL_STATIC_DRAW) == false) {
		psilog(PSILog::FAIL, "Failed creating texture mesh!");
		return false;
	}
	asset.mesh->enableVertexAttrib(shader_prog, "a_position", 3, 0, 0, GL_FLOAT);

	// Indexes
	std::vector<GLuint> indexes = {0,1,2,0,2,3};
	asset.mesh->initIndexes(indexes, GL_STATIC_DRAW);

	// Texture co-ordinates
	GLfloat s0 = -1.0f;
	GLfloat t0 = -1.0f;
	GLfloat s1 = 1.0f;
	GLfloat t1 = 1.0f;

	// Texturecoords
	std::vector<glm::vec2> texcoord = {
		glm::vec2(s0, t0),
		glm::vec2(s0, t1),
		glm::vec2(s1, t1),
		glm::vec2(s1, t0)
	};

	asset.mesh->upload_vertex_attrib_data(PSIGLMesh::bufferType::TEXCOORD,
		texcoord.size() * sizeof(glm::vec2),
		&texcoord[0],
		GL_DYNAMIC_DRAW);
	asset.mesh->enable_vertex_attrib(shader_prog, "a_texcoord", 2, 0, 0, GL_FLOAT);

	psilog(PSILog::CREATE, "Created fullscreen quad");

	return true;
}
*/
}
