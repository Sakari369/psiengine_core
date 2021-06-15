// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Simple representation of a light source.

#pragma once

#include "PSIGlobals.h"
#include "PSIOpenGL.h"

class PSILight {
	public:
		PSILight() = default;
		~PSILight() = default;

		enum class LightType {
			AMBIENT = 0,
			DIRECTIONAL = 1,
			POINT = 2,
		};

		void set_type(LightType type) {
			_type = type;
		}
		LightType get_type() {
			return _type;
		}

		void set_color(glm::vec4 color) {
			_color = color;
		}
		glm::vec4 get_color() {
			return _color;
		}

		void set_intensity(GLfloat intensity) {
			_intensity = intensity;
		}
		GLfloat get_intensity() {
			return _intensity;
		}

		void set_dir(glm::vec3 dir) {
			_dir = dir;
		}
		glm::vec3 get_dir() {
			return _dir;
		}

		void set_pos(glm::vec3 pos) {
			_pos = pos;
		}
		glm::vec3 get_pos() {
			return _pos;
		}

	private:
		// Type of light.
		LightType _type = LightType::AMBIENT;
		// Color of light.
		glm::vec4 _color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		// Direction vector for light.
		glm::vec3 _dir = glm::vec3(0.0f, 0.0f, 1.0f);
		// Position in world.
		glm::vec3 _pos = glm::vec3(0.0f, 0.0f, 0.0f);
		// Light intensity.
		GLfloat _intensity = 1.0f;
};
