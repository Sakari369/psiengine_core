#include "PSIGLTransform.h"

glm::mat4 PSIGLTransform::get_model() {
	// Translate.
	glm::mat4 translated = glm::translate(glm::mat4(1.0f), _translation);
	// Scale.
	glm::mat4 scaled = glm::scale(translated, _scaling);
	// Rotate.
	glm::mat4 x_rotated = glm::rotate(scaled,    _rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
	glm::mat4 y_rotated = glm::rotate(x_rotated, _rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 z_rotated = glm::rotate(y_rotated, _rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));

	return z_rotated;
}
