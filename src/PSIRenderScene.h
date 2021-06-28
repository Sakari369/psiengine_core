// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// A Scene that PSIGLRenderer renders. Contains all the render objects and lights.

#pragma once

#include "PSIGlobals.h"
#include "PSIOpenGL.h"
#include "PSILight.h"
#include "PSIRenderObj.h"

typedef vector<RenderObjSharedPtr> RenderObjVector;

class PSIRenderScene;
typedef shared_ptr<PSIRenderScene> RenderSceneSharedPtr;

class PSIRenderScene {
	public:
		PSIRenderScene() = default;
		~PSIRenderScene() = default;

		static RenderSceneSharedPtr create() {
			return make_shared<PSIRenderScene>();
		}

		// Inverse sort for transparent objects.
		// Sorts objects based on their Z position, objects with smaller Z are moved towards 0 index.
		// So that objects back in the scene are drawn first, in order to achieve proper transparency.
		struct depth_sort_inversed {
			bool operator() (RenderObjSharedPtr &left, RenderObjSharedPtr &right) {
				return left->get_sort_index() < right->get_sort_index();
			}
			bool operator() (RenderObjSharedPtr &left, float right) {
				return left->get_sort_index() < right;
			}
			bool operator() (float left, RenderObjSharedPtr &right) {
				return left < right->get_sort_index();
			}
		};

		// Normal sort for opaque objects
		// Sorts objects based on their Z position, objects with smaller Z are moved towards array.size()
		// So that objects front of the scene are drawn first
		struct depth_sort_normal {
			bool operator() (RenderObjSharedPtr &left, RenderObjSharedPtr &right) {
				return left->get_sort_index() > right->get_sort_index();
			}
			bool operator() (RenderObjSharedPtr &left, float right) {
				return left->get_sort_index() > right;
			}
			bool operator() (float left, RenderObjSharedPtr &right) {
				return left > right->get_sort_index();
			}
		};

		// Set sort function.
		// By default we assume all of our objects might contain transparency, and are sorted inversed.
		static struct depth_sort_inversed depth_compare_func;
		//static struct depthSortNormal depth_compare_func;

		// Append render object to scene.
		void add(RenderObjSharedPtr obj);
		// Remove passed in render object from scene.
		GLboolean remove(RenderObjSharedPtr obj);
		// Sort render objects by depth.
		void sort();

		// Reset scene.
		void reset() {
			m_render_objs.clear();
			_lights.clear();
		}

		RenderObjVector get_render_objs() {
			return m_render_objs;
		}

		void add_light(LightSharedPtr light) {
			_lights.push_back(light);
		}
		std::vector<LightSharedPtr> get_lights() {
			return _lights;
		}

		// All renderable objects in the scene.
		RenderObjVector m_render_objs;

	private:
		// Light sources in the scene.
		vector<LightSharedPtr> _lights;
};
