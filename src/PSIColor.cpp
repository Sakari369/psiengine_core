#include "PSIColor.h"

namespace PSIColor {

glm::vec4 bitmask_to_vec(GLint color) {
	GLint r = (color & Bitmask::RED) >> 16;
	GLint g = (color & Bitmask::GREEN) >> 8;
	GLint b = (color & Bitmask::BLUE);

	return glm::vec4(
		(1.0f/255.0) * r, 
		(1.0f/255.0) * g,
		(1.0f/255.0) * b,
		1.0f);
}

glm::vec4 bitmask_to_vec_hsv(GLint color) {
	GLfloat r = 1.0f/255.0f * ((color & Bitmask::RED) >> 16);
	GLfloat g = 1.0f/255.0f * ((color & Bitmask::GREEN) >> 8);
	GLfloat b = 1.0f/255.0f * ((color & Bitmask::BLUE));

	GLfloat max = std::max( {r, g, b} );
	GLfloat min = std::min( {r, g, b} );

	GLfloat h = max;
	GLfloat s = max;
	GLfloat v = max;

	GLfloat d = max - min;
	if (max == 0.0f) {
		s = 0.0f;
	} else {
		s = d / max;
	}

	if (max == min) {
		h = 0.0f; // achromatic
	} else {
		if (r == max) {
			h = (g - b) / d + (g < b ? 6.0f : 0.0f);
		} else if (g == max) {
			h = (b - r) / d + 2; 
		} else if (b == max) {
			h = (r - g) / d + 4; 
		}

		h = h / 6.0f;
	}

	return glm::vec4(h, s, v, 1.0f);
}

glm::vec4 rgb2hsv(glm::vec4 color) {
	GLfloat max = std::max( {color.r, color.g, color.b} );
	GLfloat min = std::min( {color.r, color.g, color.b} );

	GLfloat h = max;
	GLfloat s = max;
	GLfloat v = max;

	GLfloat d = max - min;
	if (max == 0.0f) {
		s = 0.0f;
	} else {
		s = d / max;
	}

	if (max == min) {
		h = 0.0f; // achromatic
	} else {
		if (color.r == max) {
			h = (color.g - color.b) / d + (color.g < color.b ? 6 : 0); 
		} else if (color.g == max) {
			h = (color.b - color.r) / d + 2; 
		} else if (color.b == max) {
			h = (color.r - color.g) / d + 4; 
		}

		h = h / 6.0f;
	}

	return glm::vec4(h, s, v, color.a);
}

std::vector<glm::vec4> create_hue_rainbow(GLuint color_count) {
	std::vector<glm::vec4> colors;
	colors.reserve(color_count);

	for (int i=0; i<color_count; i++) {
		// Interpolate to range 0 .. 360.
		GLfloat hue_angle_deg = i * (360.0f / color_count);

		// To range 1.0.
		GLfloat hue = (1.0f / 360.0f) * hue_angle_deg;
		GLfloat saturation = 1.0f;
		GLfloat value = 1.0f;
		GLfloat alpha = 1.0f;

		glm::vec4 color_hsva = glm::vec4(hue, saturation, value, alpha);
		colors.push_back(color_hsva);
	}

	return colors;
}

}
