#include "PSIRenderScene.h"

GLboolean PSIRenderScene::remove(shared_ptr<PSIRenderObj> obj) {
	// Remove object from _render_objs.
	auto it = _render_objs.erase(_render_objs.begin() + obj->get_scene_index());
	// And now loop the iterator following the last element removed.
	// Fall down all objects above the removed element.
	for (; it != _render_objs.end(); it++) {
		(*it)->set_scene_index((*it)->get_scene_index() - 1);
	}

	return true;
}

void PSIRenderScene::add(shared_ptr<PSIRenderObj> obj) {
	_render_objs.push_back(obj);
	GLuint scene_index = _render_objs.size() - 1;
	obj->set_scene_index(scene_index);
}

void PSIRenderScene::sort() {
	// Do a simple insertion sort for our rendered objects.
	for (auto it = _render_objs.begin(), end = _render_objs.end(); it != end; ++it) {
		// Search.
		auto const insert_point = std::upper_bound(_render_objs.begin(), it, 
							  (*it)->get_sort_index(), depth_compare_func);
		// Insert.
		std::rotate(insert_point, it, it + 1);
	}
}
