// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Miniaudio implementation.

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"    // Enables Vorbis decoding.

#define MA_NO_MP3
#define MA_NO_WAV
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

// The stb_vorbis implementation must come after the implementation of miniaudio.
#undef STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"