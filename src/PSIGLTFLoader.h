// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// GLTF model loader. Interfaces tiny_gltf_loader.

#pragma once

#include "PSIGLMesh.h"
#include "PSIGLShader.h"

#include "ext/tiny_gltf_loader.h"

class PSIGLTFLoader {
	private:
	public:
		PSIGLTFLoader() = default;
		~PSIGLTFLoader() = default;

		// Create GLMesh from GLTF scene.
		shared_ptr<PSIGLMesh> create_gl_mesh(const shared_ptr<PSIGLShader> &shader, const tinygltf::Scene &scene);
		// Load GLTF scene from file and create GLMesh from that.
		shared_ptr<PSIGLMesh> load_gl_mesh(const shared_ptr<PSIGLShader> &shader, std::string scene_path);
};
