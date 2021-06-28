#include "PSIGeometry.h"

namespace PSIGeometry {

void add_buffer_defaults(const GeometryDataSharedPtr &geom) {
	geom->buffers = {
		{ 
			PSIGLMesh::BufferName::POSITION,
			GL_ARRAY_BUFFER,
			static_cast<GLsizeiptr>(geom->positions.size() * sizeof(glm::vec3)),
			GL_STATIC_DRAW,
			nullptr
		},
		{ 
			PSIGLMesh::BufferName::COLOR,
			GL_ARRAY_BUFFER,
			static_cast<GLsizeiptr>(geom->colors.size() * sizeof(glm::vec4)),
			GL_DYNAMIC_DRAW,
			nullptr
		},
		{ 
			PSIGLMesh::BufferName::NORMAL,
			GL_ARRAY_BUFFER,
			static_cast<GLsizeiptr>(geom->normals.size() * sizeof(glm::vec3)),
			GL_STATIC_DRAW,
			nullptr
		},
		{ 
			PSIGLMesh::BufferName::TEXCOORD,
			GL_ARRAY_BUFFER,
			static_cast<GLsizeiptr>(geom->texcoords.size() * sizeof(glm::vec2)),
			GL_STATIC_DRAW,
			nullptr
		},
		{ 
			PSIGLMesh::BufferName::INDEX,
			GL_ELEMENT_ARRAY_BUFFER,
			static_cast<GLsizeiptr>(geom->indexes.size() * sizeof(GLuint)),
			GL_STATIC_DRAW,
			nullptr
		},
	};
	
	geom->attributes = {
		{ PSIGLMesh::BufferName::POSITION, "a_position", 3, GL_FLOAT, 0, 0, nullptr },
		{ PSIGLMesh::BufferName::COLOR,    "a_color",    4, GL_FLOAT, 0, 0, nullptr },
		{ PSIGLMesh::BufferName::NORMAL,   "a_normal",   3, GL_FLOAT, 0, 0, nullptr },
		{ PSIGLMesh::BufferName::TEXCOORD, "a_texcoord", 2, GL_FLOAT, 0, 0, nullptr },
	};
}

GeometryDataSharedPtr cube() {
	GeometryDataSharedPtr geom = PSIGeometryData::create();
	geom->positions = PSIGeometry::Cube::positions;
	geom->normals   = PSIGeometry::Cube::normals;
	geom->texcoords = PSIGeometry::Cube::texcoords;
	geom->indexes   = PSIGeometry::Cube::indexes;
	add_buffer_defaults(geom);

	return geom;
}

GeometryDataSharedPtr cube_tetrahedron() {
	GeometryDataSharedPtr geom = PSIGeometryData::create();

	geom->positions = PSIGeometry::CubeTetrahedron::positions;
	geom->texcoords = PSIGeometry::CubeTetrahedron::texcoords;
	geom->indexes   = PSIGeometry::Tetrahedron::indexes;

	// Calculate the normals.
	const std::vector<PSI::triangle> &tf = PSIGeometry::CubeTetrahedron::faces;

	glm::vec3 normal;
	normal = PSIMath::calc_tri_normal(tf[0]);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);

	normal = PSIMath::calc_tri_normal(tf[1]);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);

	normal = PSIMath::calc_tri_normal(tf[2]);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);

	normal = PSIMath::calc_tri_normal(tf[3]);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);

	add_buffer_defaults(geom);

	return geom;
}

GeometryDataSharedPtr tetrahedron() {
	GeometryDataSharedPtr geom = PSIGeometryData::create();

	geom->positions = PSIGeometry::Tetrahedron::positions;
	geom->texcoords = PSIGeometry::Tetrahedron::texcoords;
	geom->indexes   = PSIGeometry::Tetrahedron::indexes;

	// Calculate the normals.
	const std::vector<PSI::triangle> &tf = PSIGeometry::Tetrahedron::faces;

	glm::vec3 normal;
	normal = PSIMath::calc_tri_normal(tf[0]);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);

	normal = PSIMath::calc_tri_normal(tf[1]);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);

	normal = PSIMath::calc_tri_normal(tf[2]);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);

	normal = PSIMath::calc_tri_normal(tf[3]);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);
	geom->normals.push_back(normal);

	add_buffer_defaults(geom);

	return geom;
}

GeometryDataSharedPtr cuboid(GLfloat width, GLfloat height, GLfloat depth) {
	GeometryDataSharedPtr geom = PSIGeometry::Cuboid::cuboid(width, height, depth);
	add_buffer_defaults(geom);

	return geom;
}

GeometryDataSharedPtr prism(GLfloat radius, GLfloat depth) {
	GeometryDataSharedPtr geom = PSIGeometry::Prism::prism(radius, depth);
	add_buffer_defaults(geom);

	return geom;
}

GeometryDataSharedPtr cube_inverted() {
	GeometryDataSharedPtr geom = PSIGeometryData::create();
	geom->positions = PSIGeometry::Cube::positions;
	geom->normals   = PSIGeometry::Cube::normals;
	geom->texcoords = PSIGeometry::Cube::texcoords;
	geom->indexes   = PSIGeometry::Cube::indexes_ccw;
	add_buffer_defaults(geom);

	return geom;
}

GeometryDataSharedPtr plane(GLint rows, GLboolean repeat_texture) {
	GeometryDataSharedPtr geom = PSIGeometry::Plane::uniform_plane(rows, repeat_texture);
	add_buffer_defaults(geom);

	return geom;
}

GeometryDataSharedPtr icosahedron(GLint recursion) {
	GeometryDataSharedPtr geom = PSIGeometry::Icosahedron::icosahedron(recursion);
	add_buffer_defaults(geom);

	return geom;
}

// Generate a circular polygon with numPoints and radius
std::vector<glm::vec3> create_poly(GLint num_points, GLfloat angle_offset, GLfloat radius) {
	GLfloat angle_inc = TWO_PI / num_points;
	GLfloat angle_rad = glm::radians(angle_offset);

	std::vector<glm::vec3> vertexes;
	vertexes.reserve(num_points);

	// Calculate all the vertex points
	for (int i=0; i<num_points; i++) {
		// Calculate vertex
		GLfloat x = sin(angle_rad) * radius;
		GLfloat y = cos(angle_rad) * radius;
		GLfloat z = 0.0f;

		// Increase angle
		angle_rad += angle_inc;

		// Push to our vertexes pointer
		vertexes.push_back(glm::vec3(x, y, z));
	}

	return vertexes;
}

/*
GeometryDataSharedPtr sphere(GLfloat radius, GLint widthSegments, GLint heightSegments,
	                           GLfloat phiStart, GLfloat phiLength, GLfloat thetaStart, GLfloat thetaLength) {

	GeometryDataSharedPtr geom = PSIGeometryData::create();
	PSIGeometry::Sphere::sphere(radius, widthSegments, heightSegments, phiStart, phiLength, thetaStart, thetaLength);

	return geom;
}
*/

} // namespace PSIGeometry
