// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// A Scene that PSIGLRenderer renders. Contains all the render objects and lights.

#pragma once

#include "PSIGlobals.h"
#include "PSIOpenGL.h"
#include "PSILight.h"
#include "PSIRenderObj.h"

class PSIRenderScene {
	public:
		PSIRenderScene() = default;
		~PSIRenderScene() = default;

		// Inverse sort for transparent objects.
		// Sorts objects based on their Z position, objects with smaller Z are moved towards 0 index.
		// So that objects back in the scene are drawn first, in order to achieve proper transparency.
		struct depth_sort_inversed {
			bool operator() (std::shared_ptr<PSIRenderObj> &left, std::shared_ptr<PSIRenderObj> &right) {
				return left->get_sort_index() < right->get_sort_index();
			}
			bool operator() (std::shared_ptr<PSIRenderObj> &left, float right) {
				return left->get_sort_index() < right;
			}
			bool operator() (float left, std::shared_ptr<PSIRenderObj> &right) {
				return left < right->get_sort_index();
			}
		};

		// Normal sort for opaque objects
		// Sorts objects based on their Z position, objects with smaller Z are moved towards array.size()
		// So that objects front of the scene are drawn first
		struct depth_sort_normal {
			bool operator() (std::shared_ptr<PSIRenderObj> &left, std::shared_ptr<PSIRenderObj> &right) {
				return left->get_sort_index() > right->get_sort_index();
			}
			bool operator() (std::shared_ptr<PSIRenderObj> &left, float right) {
				return left->get_sort_index() > right;
			}
			bool operator() (float left, std::shared_ptr<PSIRenderObj> &right) {
				return left > right->get_sort_index();
			}
		};

		// Set sort funciton.
		// By default we assume all of our objects contain transparency (for now), and
		// are sorted inversed.
		static struct depth_sort_inversed depth_compare_func;
		//static struct depthSortNormal depth_compare_func;

		// Append render object to scene.
		void add(shared_ptr<PSIRenderObj> obj);
		// Remove passed in render object from scene.
		GLboolean remove(shared_ptr<PSIRenderObj> obj);
		// Sort render objects by depth.
		void sort();

		// Reset scene.
		void reset() {
			_render_objs.clear();
			_lights.clear();
		}

		std::vector<shared_ptr<PSIRenderObj>> get_render_objs() {
			return _render_objs;
		}

		void add_light(shared_ptr<PSILight> light) {
			_lights.push_back(light);
		}
		std::vector<shared_ptr<PSILight>> get_lights() {
			return _lights;
		}

	private:
		// Renderable objects in the scene.
		std::vector<shared_ptr<PSIRenderObj>> _render_objs;
		// Light sources in the scene.
		std::vector<shared_ptr<PSILight>> _lights;
};
