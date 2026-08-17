// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Define global types.

#pragma once

#include <string>
#include <memory>
#include <type_traits>
#include <vector>
#include <cstdint>

// GL scalar compatibility shim.
//
// These names were previously supplied by <GL/glew.h>. They are used as the
// engine's general scalar vocabulary in ~81 files, including many that never
// touched OpenGL (PSICamera, PSILight, PSIScaler, ...), and in every
// LUA_ARGS() declaration in LuaAPI.cpp. Defining them here keeps the Metal port
// from turning into an 81-file rename in the same commit.
//
// TODO: rename to psi::f32 / psi::i32 style once the Metal backend renders.
using GLfloat    = float;
using GLdouble   = double;
using GLclampf   = float;
using GLint      = int32_t;
using GLuint     = uint32_t;
using GLshort    = int16_t;
using GLushort   = uint16_t;
using GLbyte     = int8_t;
using GLubyte    = uint8_t;
// NOT bool. GLEW defined GLboolean as unsigned char, and that difference is
// visible from Lua: LuaIntf pushes an integral type as a Lua *number* but a
// C++ bool as a Lua *boolean*. Every script tests input with
// `psi.input:key_pressed(psi.KEYS.KEY_ESCAPE) == 1`, and in Lua `true == 1` is
// false -- so typedefing this to bool silently breaks all keyboard and mouse
// button handling, ESC-to-quit included.
using GLboolean  = uint8_t;
using GLsizei    = int32_t;
using GLenum     = uint32_t;
using GLbitfield = uint32_t;
using GLchar     = char;
using GLvoid     = void;
using GLintptr   = intptr_t;
using GLsizeiptr = ptrdiff_t;
// Not a real GL type -- used at PSIGLShader.cpp:72,76 as a container size type.
using GLulong    = uint64_t;

using std::unique_ptr;
using std::shared_ptr;
using std::move;
using std::make_shared;
using std::make_unique;
using std::vector;

// Needed together with enum classes
struct EnumClassHash {
    template <typename T>
    std::size_t operator()(T t) const {
        return static_cast<std::size_t>(t);
    }
};
