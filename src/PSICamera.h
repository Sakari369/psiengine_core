// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// 3D camera.

#pragma once

#include "PSIGlobals.h"
#include "PSIOpenGL.h"
#include "PSIMath.h"
#include "PSIGLUtils.h"

#include <cmath>

class PSICamera;
typedef shared_ptr<PSICamera> CameraSharedPtr;

class PSICamera {
	public:
		PSICamera() = default;
		~PSICamera() = default;

		static CameraSharedPtr create() {
			return make_shared<PSICamera>();
		}

		// Default far plane value.
		static const GLfloat DEF_FAR_PLANE;
		// Default near plane value.
		static const GLfloat DEF_NEAR_PLANE;

		// Translate position to current_pos + translation.
		void translate(const glm::vec3 &translation) {
			_pos = _pos + translation;
		}

		// Rotate camera front vector around selected axis.
		void rotate(GLfloat amount, const glm::vec3 &axis) {
			_front = glm::rotate(_front, amount, axis);
		}

		glm::vec3 inc_axis_rotation(const glm::vec3 &rot_axis_delta);
		glm::vec3 set_axis_rotation(const glm::vec3 &rot_axis);
		glm::vec3 get_axis_rotation() {
			return glm::vec3(_yaw, _pitch, _roll);
		}

		// Calculate camera front vector from yaw, roll and pitch.
		glm::vec3 calc_front();

		void set_viewport_aspect_ratio(GLfloat viewport_aspect_ratio) {
			assert(viewport_aspect_ratio > 0.0);
			_viewport_aspect_ratio = viewport_aspect_ratio;
		}
		GLfloat get_viewport_aspect_ratio() {
			return _viewport_aspect_ratio;
		}

		glm::mat4 get_looking_at_matrix() {
			return glm::lookAt(_pos, _pos + _front, _up);
		}
		glm::mat4 get_projection_matrix() {
			return glm::perspective(_fov, _viewport_aspect_ratio, _near_plane, _far_plane);
		}

		// Remove the translation component from the view matrix.
		// This happens by converting the mat4 to mat3, then back to mat4 to get the zeroed out values.
		glm::mat4 get_looking_at_matrix_without_translation() {
			return glm::mat4(glm::mat3(glm::lookAt(_pos, _pos + _front, _up)));
		}

		void set_yaw(GLfloat yaw) {
			_yaw = yaw;
		}
		GLfloat get_yaw() {
			return _yaw;
		}

		void set_pitch(GLfloat pitch) {
			_pitch = pitch;
			if (_pitch > 89.0f) {
				_pitch = 89.0f;
			} else if (_pitch < -89.0f) {
				_pitch = -89.0f;
			}
		}
		GLfloat get_pitch() {
			return _pitch;
		}

		void set_roll(GLfloat roll) {
			_roll = roll;
		}
		GLfloat get_roll() {
			return _roll;
		}

		void set_pos(const glm::vec3 &pos) {
			_pos = pos;
		}
		glm::vec3& get_pos() {
			return _pos;
		}

		void set_front(const glm::vec3 &front) {
			_front = front;
		}
		glm::vec3& get_front() {
			return _front;
		}

		void set_up(const glm::vec3 &up) {
			_up = up;
		}
		glm::vec3& get_up() {
			return _up;
		}

		// Set all pos, front and up vectors at once.
		void set_vectors(const glm::vec3 &pos, const glm::vec3 &front, const glm::vec3 &up) {
			_pos = pos;
			_front = front;
			_up = up;
		}

		void set_fov(GLfloat fov) {
			assert(fov > 0.0f && fov < 180.0f);
			_fov = glm::radians(fov);
		}
		GLfloat get_fov() {
			return glm::degrees(_fov);
		}

		void set_near_plane(GLfloat near_plane) {
			// Catch invalid setting
			assert(near_plane > 0.0f);
			_near_plane = near_plane;
		}
		GLfloat get_near_plane() {
			return _near_plane;
		}

		void set_far_plane(GLfloat far_plane) {
			assert(far_plane > _near_plane);
			_far_plane = far_plane;
		}
		GLfloat get_far_plane() {
			return _far_plane;
		}

		// Set near and far planes.
		void set_planes(GLfloat near_plane, GLfloat far_plane) {
			assert(near_plane > 0.0f);
			assert(far_plane > near_plane);
			_near_plane = near_plane;
			_far_plane = far_plane;
		}

		// Recenter camera. If reset_z_axis is false, only reset x and y position.
		void recenter(GLboolean reset_z_axis);

		// Print position vectors to output.
		void print_position_vectors();

	private:
		// Pos : where our camera view is looking from
		glm::vec3 _pos = glm::vec3(0.0f);

		// Front: where the camera is looking at
		glm::vec3 _front = glm::vec3(0.0f);

		// Vector pointing up from the camera position
		glm::vec3 _up = glm::vec3(0.0f, 1.0f, 0.0f);

		// Yaw and pitch angles, so that we can set the yaw and pitch indepedent of the front vector
		// In radians
		GLfloat _yaw = 0.0f;
		GLfloat _pitch = 0.0f;
		GLfloat _roll = 0.0f;

		GLfloat _fov = 90.0f;
		GLfloat _near_plane = DEF_NEAR_PLANE;
		GLfloat _far_plane = DEF_FAR_PLANE;

		// TODO: get default somehow from video settings ?
		GLfloat _viewport_aspect_ratio = 1680/1050.0f;
};
