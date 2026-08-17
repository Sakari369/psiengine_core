#include "PSIAABB.h"
#include "PSIGLUtils.h"

bool PSIAABB::contains_point(glm::vec3 point) {
	return (point.x > _min.x && point.x < _max.x &&
	       (point.y > _min.y && point.y < _max.y) &&
	       (point.z > _min.z && point.z < _max.z));
}

bool PSIAABB::intersect(PSIAABB &aabb) {
	glm::vec3 tmin = aabb.get_min();
	glm::vec3 tmax = aabb.get_max();

	plog_s("_max = %s", GLM_CSTR(_max));
	plog_s("_min = %s", GLM_CSTR(_min));
	plog_s("tmin = %s", GLM_CSTR(tmin));
	plog_s("tmax = %s", GLM_CSTR(tmax));

	bool x_is = _min.x < tmax.x && _max.x > tmin.x;
	plog_s("x_is = %d", x_is);

	bool y_is = _min.y < tmax.y && _max.y > tmin.y;
	plog_s("y_is = %d", y_is);

	bool z_is = _min.z < tmax.z && _max.z > tmin.z;
	plog_s("z_is = %d", z_is);

	return (_min.x < tmax.x && _max.x > tmin.x &&
	        _min.y < tmax.y && _max.y > tmin.y &&
	        _min.z < tmax.z && _max.z > tmin.z);
}

// We should add translation to both min and max
void PSIAABB::translate_to(glm::vec3 translation) {
	_min = _min + translation;
	_max = _max + translation;
}

// Basically what we want to do is ..
//
// Transform our existing minimums and maximums to the matrix provided..
// So we have to multiply each existing minimum and maximum with the matrix values
//

// This is not correct
// Does not take rotation into account correctly ..
//
// How would we go on doing this properly ?
void PSIAABB::transform_to_matrix(const glm::mat4 matrix) {
	glm::vec4 translation = matrix[3];
	glm::vec3 a_min = _min;
	glm::vec3 a_max = _max;

	plog_s("matrix = %s", GLM_CSTR(matrix));
	plog_s("_translation = %s", GLM_CSTR(translation));
	plog_s("_min = %s", GLM_CSTR(_min));
	plog_s("_max = %s", GLM_CSTR(_max));

	for(int j=0; j<3; j++ ) {
		glm::vec3 col = glm::vec3(glm::column(matrix, j));
		plog_s("col = %s", GLM_CSTR(col));

		for (int i=0; i<3; i++) {
			// Multiply our current minimum and maximum points
			// with the matrix values
			GLfloat a = col[j] * a_min[i];
			GLfloat b = col[j] * a_max[i];

			// Find the new minimums from the rotated and scaled values, 
			// add translation to the minimums
			if( a < b ) {
				plog_s("a=%.3f b=%.3f, a < b, _min[%d] = %.3f _max[%d] = %.3f", a, b, j, _min[j], j, _max[j]);
				_min[j] = translation[j] + a;
				_max[j] = translation[j] + b;
			} else {
				plog_s("a=%.3f b=%.3f, a > b, _min[%d] = %.3f _max[%d] = %.3f", a, b, j, _min[j], j, _max[j]);
				_min[j] = translation[j] + b;
				_max[j] = translation[j] + a;
			}
		}
	}

	plog_s("new _min = %s", GLM_CSTR(_min));
	plog_s("new _max = %s", GLM_CSTR(_max));
}