// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Axis Aligned Bounding Box implementation.

#pragma once

#include "PSIGlobals.h"

// This can be optimized
// http://www.yosoygames.com.ar/wp/2013/07/good-bye-axisalignedbox-hello-aabb/

class PSIAABB {
	private:
		glm::vec3 _min = glm::vec3(-0.5f, -0.5f, -0.5f);
		glm::vec3 _max = glm::vec3(0.5f, 0.5f, 0.5f);
		// See is_valid().
		bool _valid = false;

	public:
		PSIAABB() = default;
		~PSIAABB() = default;

		// Does this bounding volume contain a point in 3d space ?
		bool contains_point(glm::vec3 point);

		// Does this intersect with another AABB ?
		bool intersect(PSIAABB &aabb);
		void transform_to_matrix(glm::mat4 matrix);

		void scale_to(glm::vec3 scaling);
		void translate_to(glm::vec3 translation);

		void set_min(glm::vec3 min) { _min = min; _valid = true; }
		glm::vec3 get_min() { return _min; }

		void set_max(glm::vec3 max) { _max = max; _valid = true; }
		glm::vec3 get_max() { return _max; }

		// Do _min/_max describe real geometry, or are they still the default
		// unit cube?
		//
		// This matters because the default is not a conservative bound -- it is
		// smaller than most meshes. Frustum culling against it would discard a
		// 512-unit star ray as if it were one unit across, so anything that
		// culls has to check this first. PSIRenderObj fills the box in from
		// PSIGeometryData::positions when geometry is attached.
		bool is_valid() const { return _valid; }

		void set_bounds(const glm::vec3 &min, const glm::vec3 &max) {
			_min = min;
			_max = max;
			_valid = true;
		}

		glm::vec3 get_center() const { return (_min + _max) * 0.5f; }
		glm::vec3 get_extent() const { return (_max - _min) * 0.5f; }
};
