// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Common graphics includes: glm math, GLFW windowing and the Metal backend.
//
// Historically this header pulled in <GL/glew.h> and was the single choke point
// for OpenGL. The engine now renders with Metal (see PSIMetal.h) and GLEW is
// gone, but the filename is kept so the ~18 direct includers and everything
// reaching it through PSIMath.h / PSIGlobals.h do not have to change in the same
// commit as the backend rewrite.
//
// TODO: rename to PSIGraphics.h once the Metal backend renders.

#pragma once

// For GL types and vectors
#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
// Metal clip space is z in [0, 1]; OpenGL used [-1, 1]. Without this every
// projection matrix glm builds would be wrong by half the depth range.
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

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
// Triangle normals.
#include <glm/gtx/normal.hpp>

// GLFW still provides the window, monitor and input layer. Tell it not to pull
// in any client API headers -- there is no GL context any more.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// Metal backend and the GL scalar typedef shim.
#include "PSIMetal.h"

// TEMPORARY: inert stubs for the GL calls not yet ported. Delete along with
// PSIGLCompat.h once the last PSIGL* class is on Metal.
#include "PSIGLCompat.h"
