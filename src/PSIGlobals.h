// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Defines a global namespace that contains all of the global variables.

#pragma once

#include "PSITypes.h"
#include "PSIHelpers.h"
#include "PSIMath.h"
#include "PSILog.h"

// Our global namespace.
namespace PSI_G {
	// The name we are being called with.
	extern const char *program_name;
	// Current asset directory.
	extern const char *asset_dir;
	// Logger instance.
	extern PSILog log;
};
