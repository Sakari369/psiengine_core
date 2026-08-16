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

		// Order the scene for drawing.
		//
		// Four groups, in this order:
		//
		//   1. objects with an explicit negative set_sort_index(), by that index
		//   2. opaque objects, in the order they were added
		//   3. transparent objects, back to front by distance from the camera
		//   4. objects with an explicit non-negative set_sort_index(), by index
		//
		// Depth-sorting the opaque group is wasted work: the depth buffer already
		// resolves them, and holding them in a fixed order lets the tile-based GPU
		// discard occluded fragments instead of shading every one. Only the
		// transparent group actually needs an order, and it needs distance from
		// the camera -- the old comparator used the raw world-space Z translation,
		// so whether it came out back-to-front depended on which way the camera
		// happened to face.
		//
		// Groups 1 and 4 are the manual-placement escape hatch; the skybox uses
		// -1000 to draw first (assets/scripts/psi/obj.lua) and needs to, because
		// it is the one object in the tree with depth testing off.
		void sort(const glm::vec3 &camera_pos);

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
		// By value: this is what Lua binds (LuaAPI.cpp).
		std::vector<LightSharedPtr> get_lights() {
			return _lights;
		}
		// Draw-path accessor; the renderer copied the whole vector into the
		// context once per shader group per frame.
		const std::vector<LightSharedPtr> &get_lights_ref() const {
			return _lights;
		}

		void set_render_to_texture(bool render_to_texture) {
			_render_to_texture = render_to_texture;
		}

		bool get_render_to_texture() {
			return _render_to_texture;
		}

		// All renderable objects in the scene.
		RenderObjVector m_render_objs;

	private:
		// Light sources in the scene.
		vector<LightSharedPtr> _lights;

		bool _render_to_texture = false;

		// Draw groups; see sort().
		enum SortGroup {
			GROUP_MANUAL_FIRST = 0,
			GROUP_OPAQUE       = 1,
			GROUP_TRANSPARENT  = 2,
			GROUP_MANUAL_LAST  = 3,
		};

		// Scratch for sort(), kept as members so a per-frame sort does not
		// allocate. One key per object and one index permutation.
		struct sort_key {
			// Draw group; see sort(). Compared first.
			int group;
			// Order within the group.
			GLfloat value;

			bool operator<(const sort_key &rhs) const {
				if (group != rhs.group) {
					return group < rhs.group;
				}
				return value < rhs.value;
			}
		};
		std::vector<sort_key> _sort_keys;
		std::vector<size_t> _sort_order;
};
