// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Defines a 3D mesh that is rendered on screen.

#pragma once

#include "PSIRenderObj.h"

class PSIRenderMesh : public PSIRenderObj {
	private:
		// Do we pass normals to the mesh shader ?
		GLboolean _has_normal_matrix = true;

	public:
		PSIRenderMesh() = default;
		virtual ~PSIRenderMesh() = default;

		// Clone this mesh.
		shared_ptr<PSIRenderMesh> clone();

		// Create mesh with geometry and material and add default uniforms.
		GLboolean init();

		// Add default shader uniform variables for this mesh.
		void add_default_uniforms();

		// Create colors from the material color.
		void generate_color_data(const shared_ptr<PSIGLMaterial> &material, const shared_ptr<PSIGeometryData> &gpu_data);

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
