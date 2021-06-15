// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// STB_image implementation.

// Only compile in JPEG and PNG support.
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
