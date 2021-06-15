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

	public:
		PSIAABB() = default;
		~PSIAABB() = default;

		// Does this bounding volume contain a point in 3d space ?
		bool contains_point(glm::vec3 point);

		// Does this intersect with another AABB ?
		bool intersect(PSIAABB &aabb);
		void transform_to_matrix(const glm::mat4 matrix);

		void scale_to(glm::vec3 scaling);
		void translate_to(glm::vec3 translation);

		void set_min(glm::vec3 min) { _min = min; }
		glm::vec3 get_min() { return _min; }

		void set_max(glm::vec3 max) { _max = max; }
		glm::vec3 get_max() { return _max; }
};
