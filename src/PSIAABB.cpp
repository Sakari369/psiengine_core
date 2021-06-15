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

/*
| 00 04 08 12 |
| 01 05 09 13 |
| 02 06 10 14 |
| 03 07 11 15 |

[ 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 ]
*/

/*"
void PSIAABB::transform_to_matrix(const glm::mat4 matrix) {
	// Get the first column vector
	//GLfloat xa = glm::column(matrix, 0) * (_origo.x - _radius.x/2.0);
	//
	// Are we calculating the sum of one matrix row or what ?
	glm::vec4 col0 = glm::column(matrix, 0);
	glm::vec4 col1 = glm::column(matrix, 1);
	glm::vec4 col2 = glm::column(matrix, 2);

	plog_s("matrix = %s", GLM_CSTR(matrix));

	plog_s("col0 = %s", GLM_CSTR(col0));
	plog_s("col1 = %s", GLM_CSTR(col1));
	plog_s("col2 = %s", GLM_CSTR(col2));

	GLfloat rs = col0[0] + col0[1] + col0[2] + col0[3];
	GLfloat us = col1[0] + col1[1] + col1[2] + col1[3];
	GLfloat bs = col2[0] + col2[1] + col2[2] + col2[3];

	GLfloat xa = rs * _min.x;
	GLfloat xb = rs * _max.x;

	GLfloat ya = us * _min.y;
	GLfloat yb = us * _max.y;

	GLfloat za = bs * _min.z;
	GLfloat zb = bs * _max.z;

	glm::vec3 translation = glm::vec3(matrix[3]);

	glm::vec3 xmin = glm::vec3(std::min(xa, xb));
	glm::vec3 xmax = glm::vec3(std::max(xa, xb));
	plog_s("xmin = %s xmax = %s", GLM_CSTR(xmin), GLM_CSTR(xmax));

	glm::vec3 ymin = glm::vec3(std::min(ya, yb));
	glm::vec3 ymax = glm::vec3(std::max(ya, yb));
	plog_s("ymin = %s ymax = %s", GLM_CSTR(ymin), GLM_CSTR(ymax));

	glm::vec3 zmin = glm::vec3(std::min(za, zb));
	glm::vec3 zmax = glm::vec3(std::max(za, zb));
	plog_s("zmin = %s zmax = %s", GLM_CSTR(zmin), GLM_CSTR(zmax));

	glm::vec3 new_min = xmin + ymin + zmin + translation;
	glm::vec3 new_max = xmax + ymax + zmax + translation;

	plog_s("new_min = %s", GLM_CSTR(new_min));
	plog_s("new_max = %s", GLM_CSTR(new_max));
	plog_s("translation.x = %f translation.y = %f .z = %f", translation.x, translation.y, translation.z);
}
*/