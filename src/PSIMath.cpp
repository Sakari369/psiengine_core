#include "PSIMath.h"

namespace PSIMath {

/*
GLfloat smoothstep(GLfloat edge0, GLfloat edge1, GLfloat x)
{
	// Scale, bias and saturate x to 0..1 range
	x = clamp((x - edge0)/(edge1 - edge0), 0.0, 1.0); 
	// Evaluate polynomial
	return x*x*(3 - 2*x);
}

GLfloat smootherstep(GLfloat edge0, GLfloat edge1, GLfloat x)
{
    // Scale, and clamp x to 0..1 range
    x = clamp((x - edge0)/(edge1 - edge0), 0.0, 1.0);
    // Evaluate polynomial
    return x*x*x*(x*(x*6 - 15) + 10);
}
*/

GLfloat angle_rad_between_points(const GLfloat x1, const GLfloat y1,
                                 const GLfloat x2, const GLfloat y2) {
	GLfloat angle_rad;

	GLfloat delta_x = x2 - x1;
	GLfloat delta_y = y2 - y1;

	// Vertically on the same line
	if (delta_x == 0) {
		// Angle is always TWO_PI/4.0
		// Fourth of a full circle
		if (delta_y >= 0) {
			angle_rad = HALF_PI;
		} else {
			// Or -TWO_PI/4.0
			angle_rad = -HALF_PI;
		}
	} else {
		// We know the length of the triangle
		// that is formed from the differences of x1, y1 -- x2, y2
		// So we calculate the angle with reverse tangent from
		// the ratios of the lengths
		angle_rad = atan(delta_y/delta_x);

		if (delta_x < 0) {
			// Going below 0
			// is the same as adding a half circle to the value
			angle_rad = angle_rad + M_PI;
		}
	}

	// Keep the value positive
	
	// If the angle is below 0, it is the same
	// as adding a whole circle to the angle
	if (angle_rad < 0) {
		angle_rad = angle_rad + TWO_PI;
	}

	return angle_rad;
};

glm::vec2 calc_poly_vertex(GLuint num_points, GLfloat radius, GLfloat angle_deg, GLuint vertex_index) {
	GLfloat angle_rad = glm::radians(angle_deg) + ((TWO_PI / num_points) * vertex_index);
	GLfloat x = (sin(angle_rad) * radius);
	GLfloat y = (cos(angle_rad) * radius);

	return glm::vec2(x, y);
}

}
