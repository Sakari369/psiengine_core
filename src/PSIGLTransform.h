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

		// Model matrix cache, and the values it was built from.
		//
		// get_model() runs once per object per frame and costs three sin/cos
		// pairs and four 4x4 composes. Most objects in most scenes do not move
		// between frames, so it is worth not rebuilding it -- but a dirty flag
		// on the setters cannot work here: get_translation()/get_scaling()/
		// get_rotation() hand out non-const references, Lua mutates the vectors
		// through them in place (psi/grids.lua, game/player.lua, gltf_models.lua
		// and others), and psi.ffi writes through a raw pointer. No mutator hook
		// sees any of that.
		//
		// Comparing the inputs does, whatever route they were written by: nine
		// float compares against three matrix builds. The cost when something
		// did move is those nine compares.
		mutable glm::mat4 _model_cache = glm::mat4(1.0f);
		mutable glm::vec3 _cached_translation = glm::vec3(0.0f);
		mutable glm::vec3 _cached_scaling = glm::vec3(0.0f);
		mutable glm::vec3 _cached_rotation = glm::vec3(0.0f);
		mutable bool _model_cached = false;

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
