#include "PSIGeometryData.h"
#include "PSIGLUtils.h"

void PSIGeometryData::print_data() {
	PSIGLUtils::print_vectors_of_glm("positions", positions);
	PSIGLUtils::print_vectors_of_glm("colors", colors);
	PSIGLUtils::print_vectors_of_glm("normals", normals);
	PSIGLUtils::print_vectors_of_glm("texcoords", texcoords);
	PSIGLUtils::print_vectors("indexes", indexes);
}
