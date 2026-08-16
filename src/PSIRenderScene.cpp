#include "PSIRenderScene.h"

GLboolean PSIRenderScene::remove(RenderObjSharedPtr obj) {
	// Remove object from _render_objs.
	auto it = m_render_objs.erase(m_render_objs.begin() + obj->get_scene_index());
	// And now loop the iterator following the last element removed.
	// Fall down all objects above the removed element.
	for (; it != m_render_objs.end(); it++) {
		(*it)->set_scene_index((*it)->get_scene_index() - 1);
	}

	return true;
}

void PSIRenderScene::add(RenderObjSharedPtr obj) {
	// Push to our total collection.
	m_render_objs.push_back(obj);
	GLuint scene_index = m_render_objs.size() - 1;
	obj->set_scene_index(scene_index);
}

void PSIRenderScene::sort(const glm::vec3 &camera_pos) {
	const size_t count = m_render_objs.size();
	if (count < 2) {
		return;
	}

	// Read every sort key exactly once.
	//
	// The insertion sort this replaces asked each object for its key O(n log n)
	// times through upper_bound, and get_sort_index() is a branch plus a
	// shared_ptr dereference per call. One pass up front is cheaper even when a
	// sort does happen.
	//
	// A dirty flag is not an option: the key is derived from the transform, and
	// scripts move objects by mutating the vec3 that get_translation() returns by
	// reference, or through psi.ffi's raw pointer. Neither route goes past a
	// setter. See PSIGLTransform for the same problem and the same answer.
	_sort_keys.resize(count);
	bool ordered = true;
	for (size_t i = 0; i < count; i++) {
		PSIRenderObj *obj = m_render_objs[i].get();
		sort_key &key = _sort_keys[i];

		if (obj->has_sort_index()) {
			const GLfloat index = obj->get_sort_index();
			key.group = (index < 0.0f) ? GROUP_MANUAL_FIRST : GROUP_MANUAL_LAST;
			key.value = index;
		} else if (obj->wants_blending()) {
			// Back to front: the farthest has to be drawn first, so negate the
			// distance and sort ascending with everything else.
			//
			// Distance from the camera, not the world Z translation the old
			// comparator used -- with a camera that can face any direction, raw
			// Z gives back-to-front only by luck.
			const glm::vec3 delta = obj->get_transform().get_translation() - camera_pos;
			key.group = GROUP_TRANSPARENT;
			key.value = -glm::dot(delta, delta);
		} else {
			// Opaque: the depth buffer decides, so any order draws the same
			// picture. A constant key plus a stable sort keeps the order the
			// script added them in.
			key.group = GROUP_OPAQUE;
			key.value = 0.0f;
		}

		if (i > 0 && key < _sort_keys[i - 1]) {
			ordered = false;
		}
	}

	if (ordered) {
		// Already in order. This is the common case once the opaque group stops
		// being depth-sorted: it never needs reordering at all, and objects in
		// the transparent group drift smoothly.
		return;
	}

	// Sort an index permutation, so the keys stay put and the comparator never
	// touches an object again.
	//
	// stable_sort, because objects with equal keys have to keep their relative
	// order -- the whole opaque group shares one key, and coincident transparent
	// geometry would otherwise flip draw order from frame to frame.
	_sort_order.resize(count);
	for (size_t i = 0; i < count; i++) {
		_sort_order[i] = i;
	}

	const std::vector<sort_key> &keys = _sort_keys;
	std::stable_sort(_sort_order.begin(), _sort_order.end(),
	                 [&keys](size_t left, size_t right) {
		return keys[left] < keys[right];
	});

	RenderObjVector sorted;
	sorted.reserve(count);
	for (size_t i = 0; i < count; i++) {
		sorted.push_back(m_render_objs[_sort_order[i]]);
	}
	m_render_objs.swap(sorted);

	// Rewrite the scene indexes.
	//
	// The old sort left them pointing at where each object used to be, so
	// remove() erased whatever had since moved into that slot. No shipped script
	// calls remove(), which is why it went unnoticed.
	for (size_t i = 0; i < count; i++) {
		m_render_objs[i]->set_scene_index((GLuint)i);
	}
}
