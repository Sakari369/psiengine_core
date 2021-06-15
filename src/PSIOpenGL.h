// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// All OpenGL related includes.

#pragma once

// For GL types and vectors
#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL

#include <stack>

#include <glm/glm.hpp> 
// translate, rotate, scale, perspective.
#include <glm/gtc/matrix_transform.hpp>
// matrix inversion.
#include <glm/gtc/matrix_inverse.hpp> 
// matrix row/column access.
#include <glm/gtc/matrix_access.hpp>
// value_ptr.
#include <glm/gtc/type_ptr.hpp>
// Quaternions.
#include <glm/gtc/quaternion.hpp>
// Printing.
#include <glm/gtx/string_cast.hpp>
// Angles.
#include <glm/gtx/vector_angle.hpp>

// Glew and GLFW
#include <GL/glew.h>
#include <GLFW/glfw3.h>
