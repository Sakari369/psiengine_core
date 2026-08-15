// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Defines a global namespace that contains all of the global variables.

#pragma once

#include "PSITypes.h"
#include "PSIHelpers.h"
#include "PSIMath.h"
#include "PSILog.h"

class PSIMetalContext;

// Our global namespace.
namespace PSI_G {
	// The name we are being called with.
	extern const char *program_name;
	// Current asset directory.
	extern const char *asset_dir;
	// Logger instance.
	extern PSILog log;

	// Active Metal context, owned by PSIVideo and set during its init().
	//
	// Global because PSIGLShader, PSIGLMesh and PSIGLTexture all need the device
	// and the current command encoder, and they are constructed directly from
	// Lua (PSIGLShader(), PSIGLMesh(), ...) with no way to pass one in. The GL
	// versions reached implicit global context state for the same reason.
	extern PSIMetalContext *metal_ctx;
}
