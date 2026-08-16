// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Miniaudio implementation.

// Vorbis.
//
// miniaudio decodes WAV, FLAC and MP3 on its own but not Vorbis, which needs
// stb_vorbis linked in this translation unit -- header first, implementation
// after miniaudio's, which is the order stb_vorbis requires.
//
// This was commented out, and the engine's one audio asset is an .ogg, so
// ma_decoder_init_file failed on it every time and audio.lua played silence.
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#define MA_NO_WAV
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

// The stb_vorbis implementation must come after the implementation of miniaudio.
#undef STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
