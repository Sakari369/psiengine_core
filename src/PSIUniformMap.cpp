#include "PSIUniformMap.h"

void PSIUniformMap::print_maps() {
	GLfloat_map.print_map();
	GLint_map.print_map();
	GLboolean_map.print_map();
	vec3_map.print_map();
	vec4_map.print_map();
	mat3_map.print_map();
	mat4_map.print_map();
}

void PSIUniformMap::clear() {
	GLfloat_map.map.clear();
	GLint_map.map.clear();
	GLboolean_map.map.clear();
	vec3_map.map.clear();
	vec4_map.map.clear();
	mat3_map.map.clear();
	mat4_map.map.clear();
}
