// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// General math functions and definitions.

#pragma once

#include <iostream>
#include <math.h>

#include "PSIOpenGL.h"

namespace PSI {
	// Triangle with three points.
	struct triangle {
		glm::vec3 p[3];

		glm::vec3& operator[] (int index) {
			return p[index];
		}

		const glm::vec3& operator[] (int index) const {
			return p[index];
		}
	};

	// Indexes for a triangle.
	struct tri_indexes {
		GLint i[3];

		GLint operator[] (int index) {
			return i[index];
		}

		const GLint operator[] (int index) const {
			return i[index];
		}
	};

	// Quad made out of two triangles.
	struct quad {
		PSI::triangle tri[2];

		PSI::triangle& operator[] (int index) {
			return tri[index];
		}

		const PSI::triangle& operator[] (int index) const {
			return tri[index];
		}
	};

	// Quad with four vertices.
	struct quad_buf {
		glm::vec3 p[4];

		glm::vec3& operator[] (int index) {
			return p[index];
		}

		const glm::vec3& operator[] (int index) const {
			return p[index];
		}
	};
}

// TODO: define these as constants, and under PSI::math namespace ?
// Unit lengths. 
// Use 1 meter as the base unit.
#define UL_M 		1.0f
#define UL_KM		(UL_M * 1000.0f)
#define UL_CM		(UL_M * 0.01f)
#define UL_MM		(UL_M * 0.001f)

#define L_M(x)		(x * UL_M)
#define L_CM(x)		(x * UL_CM)
#define L_KM(x)		(x * UL_KM)
#define L_MM(x)		(x * UL_MM)

#define TWO_PI		(2.0f*M_PI)
#define HALF_PI		(M_PI/2.0f)

// Phi == (1 + sqrt(5)) / 2.
#define PHI		((1 + 2.23606797749979) / 2)
#define GOLDEN_RATIO    PHI

#undef NELEMS
#define NELEMS(a)	(sizeof(a)/sizeof(a[0]))

// TODO: don't use these anymore
namespace PSIMath {
template <typename Type>
struct pos {
	Type x;
	Type y;
};

template <typename Type>
struct size {
	Type w;
	Type h;
};

template <typename Type>
struct velocity {
	Type speed;
	Type direction;
};

// Calculate angle in radians between two co-ordinates.
extern GLfloat angle_rad_between_points(const GLfloat x1, const GLfloat y1, const GLfloat x2, const GLfloat y2);

// Calculate a vertex point of a closed N -polygon.
extern glm::vec2 calc_poly_vertex(GLuint num_points, GLfloat radius, GLfloat angle_deg, GLuint vertex_index);

// Angle unit conversions.
// Limit values.
template <typename Type> inline
constexpr Type clamp(Type x, const Type low, const Type high) {
	return x > high ? high : (x < low ? low : x);
}

template <typename Type> inline
constexpr Type min_limit(Type x, const Type low) {
	return x < low ? low : x;
}

template <typename Type> inline
constexpr Type max_limit(Type x, const Type high) {
	return x > high ? high : x;
}

// Linear interpolation.
template<typename Type>
constexpr Type lerp(float t, const Type &a, const Type &b){
    return a * (1.0f - t) + b * t;
}

// For inverting values and returning the inverted value.
constexpr GLboolean invert(GLboolean &val) {
	return (val = !val);
}

constexpr GLfloat invert(GLfloat &val) {
	return (val = -val);
}

constexpr GLint invert(GLint &val) {
	return (val = -val);
}

}
