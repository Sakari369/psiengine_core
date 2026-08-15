// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Enum constants inherited from OpenGL.
//
// These are NOT a compatibility shim -- the shim of inert gl* function stubs
// that stood here during the Metal port is gone, along with the last call into
// it. What remains are numeric constants that are genuinely part of the
// engine's data model, and in two cases part of its published API:
//
//   - Primitive modes. PSIGLMesh::set_draw_mode() is bound to Lua and scripts
//     pass the raw number: assets/scripts/normal_vis.lua calls
//     set_draw_mode(0), meaning GL_POINTS. PSIGLMesh translates these to
//     MTLPrimitiveType, so the VALUES are a contract and must not be renumbered.
//
//   - Buffer targets, usage hints and component types. PSIGeometry::add_buffer_defaults()
//     fills gl_buffer_info / gl_vertex_attribute with these, and every one of
//     the nine procedural geometry generators produces them. They describe
//     intent ("this is the index buffer", "these are floats") and are
//     interpreted by PSIGLMesh.
//
//   - Texture formats and targets, used by PSIGLTexture's own TexFormat flags.
//
// They keep their original OpenGL values so nothing that round-trips through
// Lua or through the geometry data changes meaning.

#pragma once

// Booleans.
#define GL_FALSE 0
#define GL_TRUE  1

// Primitive modes. Lua passes these as bare integers -- do not renumber.
#define GL_POINTS         0x0000
#define GL_LINES          0x0001
#define GL_LINE_LOOP      0x0002
#define GL_LINE_STRIP     0x0003
#define GL_TRIANGLES      0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TRIANGLE_FAN   0x0006

// Component types, as used by the vertex attribute descriptors.
#define GL_BYTE           0x1400
#define GL_UNSIGNED_BYTE  0x1401
#define GL_SHORT          0x1402
#define GL_UNSIGNED_SHORT 0x1403
#define GL_INT            0x1404
#define GL_UNSIGNED_INT   0x1405
#define GL_FLOAT          0x1406

// Buffer targets and usage hints.
#define GL_ARRAY_BUFFER         0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW          0x88E4
#define GL_DYNAMIC_DRAW         0x88E8

// Pixel formats.
#define GL_DEPTH_COMPONENT   0x1902
#define GL_RED               0x1903
#define GL_RGB               0x1907
#define GL_RGBA              0x1908
#define GL_SRGB8_ALPHA8      0x8C43

#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3

// Texture targets and sampler parameters.
#define GL_TEXTURE_2D                  0x0DE1
#define GL_TEXTURE_CUBE_MAP            0x8513
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X 0x8515
#define GL_TEXTURE_2D_MULTISAMPLE      0x9100

#define GL_NEAREST               0x2600
#define GL_LINEAR                0x2601
#define GL_LINEAR_MIPMAP_LINEAR  0x2703
#define GL_REPEAT                0x2901
#define GL_CLAMP_TO_EDGE         0x812F
#define GL_CLAMP_TO_BORDER       0x812D

#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF

// Render state, still referenced by PSIGLRenderer's draw-mode handling.
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND      0x0BE2
#define GL_LESS       0x0201
#define GL_FILL       0x1B02
#define GL_LINE       0x1B01
#define GL_VIEWPORT   0x0BA2
