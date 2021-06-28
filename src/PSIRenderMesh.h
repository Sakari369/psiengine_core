// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Defines a 3D mesh that is rendered on screen.

#pragma once

#include "PSIRenderObj.h"

class PSIRenderMesh;
typedef shared_ptr<PSIRenderMesh> RenderMeshSharedPtr;

class PSIRenderMesh : public PSIRenderObj {
	private:
		// Do we pass normals to the mesh shader ?
		GLboolean _has_normal_matrix = true;

	public:
		PSIRenderMesh() = default;
		virtual ~PSIRenderMesh() = default;

		static RenderMeshSharedPtr create() {
			return make_shared<PSIRenderMesh>();
		}

		// Clone this mesh.
		RenderMeshSharedPtr clone() {
			RenderMeshSharedPtr clone_mesh = make_shared<PSIRenderMesh>(*this);
			// Need to re-initialize.
			clone_mesh->init();
			return clone_mesh;
		}

		// Create mesh with geometry and material and add default uniforms.
		GLboolean init();

		// Create colors from the material color.
		void generate_color_data(const GLMaterialSharedPtr &material, const GeometryDataSharedPtr &gpu_data);

		// For differentating from PSIRenderObj.
		void print_id() {
			std::cout << "PSIRenderMesh" << std::endl;
		}

		void set_has_normal_matrix(GLboolean has_normal_matrix) {
			_has_normal_matrix = has_normal_matrix;
		}
		GLboolean get_has_normal_matrix() {
			return _has_normal_matrix;
		}
};
