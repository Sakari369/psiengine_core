// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// 3D transformation class.

#pragma once

#include <iostream>
#include <math.h>

#include "PSIGlobals.h"
#include "PSIGLUtils.h"

class PSIGLTransform {
	private:
		// Translation vector.
		glm::vec3 _translation;
		// Scaling vector.
		glm::vec3 _scaling;
		// Rotation vector.
		glm::vec3 _rotation;

	public:
		PSIGLTransform(const glm::vec3 &translation = glm::vec3(0.0f, 0.0f, 0.0f), 
			       const glm::vec3 &scaling     = glm::vec3(1.0f, 1.0f, 1.0f), 
			       const glm::vec3 &rotation    = glm::vec3(0.0f, 0.0f, 0.0f)) :
			       _translation(translation),
			       _scaling(scaling),
			       _rotation(rotation) 
			       {}
		~PSIGLTransform() = default;

		// Get translated, scaled and rotated model matrix.
		glm::mat4 get_model();

		void set_translation(glm::vec3 &translation) {
			_translation = translation;
		}
		glm::vec3& get_translation() {
			return _translation;
		}

		void set_scaling(glm::vec3 &scaling) {
			_scaling = scaling;
		}
		glm::vec3& get_scaling() {
			return _scaling;
		}

		// Rotation in radians.
		void set_rotation(glm::vec3 &rotation) {
			_rotation = rotation;
		}
		glm::vec3& get_rotation() {
			return _rotation;
		}

		// Rotation in degrees.
		glm::vec3 get_rotation_deg() {
			return glm::degrees(_rotation);
		}
		void set_rotation_deg(glm::vec3 &rotation_deg) {
			_rotation = glm::radians(rotation_deg);
		}
		void add_rotation_deg(glm::vec3 &rotation) {
			_rotation += glm::radians(rotation);
		}

		// interpolate our values from from another (previous) transform.
		void interpolate_from(PSIGLTransform &transform, GLfloat interpolation) {
			GLfloat one_minus_ip = 1.0f - interpolation;
			_translation = _translation * interpolation + transform.get_translation() * one_minus_ip;
		}
};
