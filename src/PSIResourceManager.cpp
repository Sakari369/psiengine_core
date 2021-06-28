#include "PSIResourceManager.h"

ShaderSharedPtr PSIResourceManager::load_shader(std::string name,
                                                        std::string vert_shader_path,
                                                        std::string frag_shader_path,
                                                        std::string geom_shader_path) {
	auto shader = PSIGLShader::create();
	if (shader != nullptr) {
		shader->set_name(name);

		if (shader->create_program() == PSIGLShader::ShaderDefs::INVALID_SHADER) {
			return nullptr;
		}

		// Load vertex shader from file.
		if (shader->add_from_file(PSIGLShader::ShaderType::VERTEX, vert_shader_path) == false) {
			fprintf(stderr, "Failed adding vertex shader from path '%s'\n", vert_shader_path.c_str());
			return nullptr;
		}

		// Load fragment shader from file.
		if (shader->add_from_file(PSIGLShader::ShaderType::FRAGMENT, frag_shader_path) == false) {
			fprintf(stderr, "Failed adding fragment shader from path '%s'\n", frag_shader_path.c_str());
			return nullptr;
		}

		// Load optional geometry shader from file.
		if (!geom_shader_path.empty()) {
			if (shader->add_from_file(PSIGLShader::ShaderType::GEOMETRY, geom_shader_path) == false) {
				fprintf(stderr, "Failed adding geometry shader from path '%s'\n", geom_shader_path.c_str());
				return nullptr;
			}
		}

		// Compile shader.
		if (shader->compile() == PSIGLShader::ShaderDefs::INVALID_SHADER) {
			fprintf(stderr, "Failed compiling shader '%s'\n", name.c_str());
			return nullptr;
		}

		// Add our uniform locations from the active variables.
		shader->add_uniforms();

		// Add to our shader collection.
		_shaders[name] = shader;
	}

	return shader;
}