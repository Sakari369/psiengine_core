// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// GLTF model loader. Interfaces tiny_gltf_loader.

#pragma once

#include "PSIGLMesh.h"
#include "PSIGLShader.h"

#include "ext/tiny_gltf_loader.h"

class PSIGLTFLoader;
typedef shared_ptr<PSIGLTFLoader> GLTFLoaderSharedPtr;

class PSIGLTFLoader {
	private:
	public:
		PSIGLTFLoader() = default;
		~PSIGLTFLoader() = default;

		static GLTFLoaderSharedPtr create() {
			return make_shared<PSIGLTFLoader>();
		}

		// Create GLMesh from GLTF scene.
		GLMeshSharedPtr create_gl_mesh(const ShaderSharedPtr &shader, const tinygltf::Scene &scene);
		// Load GLTF scene from file and create GLMesh from that.
		GLMeshSharedPtr load_gl_mesh(const ShaderSharedPtr &shader, std::string scene_path);
};
