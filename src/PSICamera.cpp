#include "PSICamera.h"

// Use base units
const GLfloat PSICamera::DEF_NEAR_PLANE = 1    * UL_CM;
const GLfloat PSICamera::DEF_FAR_PLANE  = 1000 * UL_M;

void PSICamera::recenter(GLboolean reset_z_axis) {
	glm::vec3 new_pos = glm::vec3(0.0f, 0.0f, 0.0f);
	// Only center x and y ?
	if (reset_z_axis == false) {
		new_pos.z = _pos.z;
	}
	set_pos(new_pos);
}

glm::vec3 PSICamera::inc_axis_rotation(const glm::vec3 &rot_axis_delta) {
	set_yaw	 (_yaw   + rot_axis_delta.x);
	set_pitch(_pitch + rot_axis_delta.y);
	set_roll (_roll  + rot_axis_delta.z);

	return calc_front();
}

glm::vec3 PSICamera::set_axis_rotation(const glm::vec3 &rot_axis_delta) {
	set_yaw	 (rot_axis_delta.x);
	set_pitch(rot_axis_delta.y);
	set_roll (rot_axis_delta.z);

	return calc_front();
}

glm::vec3 PSICamera::calc_front() {
	GLfloat yaw   = glm::radians(_yaw);
	GLfloat pitch = glm::radians(_pitch);

	glm::vec3 dir;
	dir.x = cos(yaw) * cos(pitch);
	dir.y = sin(pitch);
	dir.z = sin(yaw) * cos(pitch);

	_front = glm::normalize(dir);

	return _front;
}

void PSICamera::print_position_vectors() {
	// Print these out in C-formatted style
	std::cout << "{\n" \
		  << "\t" << glm::to_string(_pos) << "," << std::endl \
		  << "\tvec3(" << _yaw << ", " << _pitch << ", " << _roll << ")," << std::endl \
		  << "\t" << glm::to_string(_front) << "," << std::endl \
		  << "\t" << glm::to_string(_up)    << "\n}" << std::endl;
}
