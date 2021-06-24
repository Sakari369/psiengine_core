// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// OpenGL utility functions.

#pragma once

#include <stdlib.h>
#include <iostream>
#include <string>
#include <iomanip> 

#include "PSIGlobals.h"
#include "PSIOpenGL.h"
#include "PSIFileUtils.h"

#define check_gl_error()	PSIGLUtils::check_error(__FILE__, __LINE__)
#define print_glm_vecs(x)	PSIGLUtils::print_vectors_of_glm(#x, x)
#define print_vecs(x)	        PSIGLUtils::print_vectors(#x, x)
#define print_vec(x)	        PSIGLUtils::print_glm_vec(#x, x)
#define print_vec_named(n, x)	PSIGLUtils::print_glm_vec(n, x)
#define GLM_CSTR(x)		glm::to_string(x).c_str()

namespace PSIGLUtils {
	template <typename T>
	void print_glm_vec(std::string name, T vec) {
		std::cout << name << "=" << glm::to_string(vec) << std::endl;
	}

	template <typename T>
	void print_vectors_of_glm(std::string name, std::vector<T> vectors) {
		std::cout << name << "(" << vectors.size() << ") =" << std::endl;
		for (T vec : vectors) {
			std::cout << glm::to_string(vec) << std::endl;
		}
	}

	template <typename T>
	void print_vectors(std::string name, std::vector<T> vectors) {
		std::cout << name << "(" << vectors.size() << ") =" << "\n";
		for (T val : vectors) {
			std::cout << val << ", ";
		}

		std::cout << std::endl;
	}

	void print_mat4(glm::mat4 const &matrix);
	GLboolean check_error(const char *file, int line);

	//bool createFullscreenQuadInAsset(PSIRenderObj::render_asset &asset);
};
