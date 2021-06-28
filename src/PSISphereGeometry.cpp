#include "PSISphereGeometry.h"

namespace PSIGeometry {

GeometryDataSharedPtr sphere(GLfloat radius, GLint widthSegments, GLint heightSegments) {

	GeometryDataSharedPtr geom = PSIGeometryData::create();

	plog_s("creating sphere!, radius = %f widthSegments = %d heightSegments = %d", radius, widthSegments, heightSegments);

	//widthSegments = PSIMath::maxLimit(3,  widthSegments);
	//heightSegments = PSIMath::maxLimit(2, heightSegments);

	GLfloat phiStart = 0;
	GLfloat phiLength = TWO_PI;
	GLfloat thetaStart = 0;
	GLfloat thetaLength = M_PI;
	GLfloat thetaEnd = thetaStart + thetaLength;

	GLint vertexCount = ((widthSegments + 1) * (heightSegments + 1));

	geom->positions.reserve(vertexCount);
	geom->normals.reserve(vertexCount);
	//var uvs = new THREE.BufferAttribute( new Float32Array( vertexCount * 2 ), 2 );

	//std::vector<GLint> positions;
	glm::vec3 normal;

	GLint index = 0;
	GLuint positions[heightSegments+1][widthSegments+1];

	for (int y=0; y<=heightSegments; y++) {
		GLfloat v = y / heightSegments;
		for (int x=0; x<=widthSegments; x++) {
			GLfloat u = x / widthSegments;
			
			GLfloat px = -radius * cos( phiStart + u * phiLength ) * sin( thetaStart + v * thetaLength );
			GLfloat py = radius  * cos( thetaStart + v * thetaLength );
			GLfloat pz = radius  * sin( phiStart + u * phiLength ) * sin( thetaStart + v * thetaLength );

			glm::vec3 vertex = glm::vec3(px, py, pz);
			normal = glm::normalize(vertex);

			geom->positions.push_back(vertex);
			geom->normals.push_back(normal);

			//uvs.setXY( index, u, 1 - v );
			//vertexRow.push_back(index);

			positions[y][x] = index;
			index++;
		}
		// We would need to do this positions as a two dimensional array
		// where we have positions[y][x]
		//
		// Can we allocate this array beforehand ?
		//
		//positions.push_bacK(vertexRow);
	}

	for (int y = 0; y<heightSegments; y++) {
		for (int x = 0; x<widthSegments; x++) {
			GLuint v1 = positions[ y ][ x + 1 ];
			GLuint v2 = positions[ y ][ x ];
			GLuint v3 = positions[ y + 1 ][ x ];
			GLuint v4 = positions[ y + 1 ][ x + 1 ];

			if ( y != 0 || thetaStart > 0 ) {
				geom->indexes.push_back(v1);
				geom->indexes.push_back(v2);
				geom->indexes.push_back(v4);
			}

			if ( y != heightSegments - 1 || thetaEnd < M_PI ) {
				geom->indexes.push_back(v2);
				geom->indexes.push_back(v3);
				geom->indexes.push_back(v4);
			}
		}
	}

	PSIGLUtils::printGLMVector("geom->positions", geom->positions);
	PSIGLUtils::printGLMVector("geom->normals", geom->normals);
	PSIGLUtils::printVector("geom->indexes", geom->indexes);

	return geom;
}
} // namespace PSIGeometry
