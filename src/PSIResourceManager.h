// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Resource loading. Currently only loads and holds shaders.

#pragma once

#include <stdio.h>
#include <wchar.h>

#include "PSIGlobals.h"
#include "PSIGLShader.h"
#include "PSIMath.h"

class PSIResourceManager {
	public:
		PSIResourceManager() = default;
		~PSIResourceManager() = default;

		// Load shader from vertex, fragment and optional geometry shader files.
		shared_ptr<PSIGLShader> load_shader(std::string name, std::string vert_shader_path,
		                                    std::string frag_shader_path, std::string geom_shader_path = "");
		// Get shader with name.
		shared_ptr<PSIGLShader> get_shader(std::string name) {
			assert(_shaders.count(name) > 0);
			return _shaders[name];
		}

	private:
		// Collection of shaders, mapped by name.
		std::unordered_map <std::string, shared_ptr<PSIGLShader>> _shaders;
};
