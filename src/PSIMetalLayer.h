// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// The one piece of the Metal backend that has to be Objective-C++.
//
// metal-cpp covers Metal and most of QuartzCore, but attaching a CAMetalLayer to
// a window means touching NSWindow/NSView, which is AppKit. This is the whole
// interface to that -- everything else stays in plain C++.

#pragma once

struct GLFWwindow;

namespace PSIMetal {

// Creates a CAMetalLayer, points it at the given MTL::Device, and installs it as
// the GLFW window's backing layer.
//
// Takes and returns void* so this header stays free of both metal-cpp and
// Objective-C types; the caller reinterpret_casts the result back to
// CA::MetalLayer*. Returns nullptr on failure.
//
// contents_scale should be the monitor content scale, so the drawable is sized
// in physical pixels on a retina display.
void *attach_metal_layer(GLFWwindow *window, void *mtl_device, double contents_scale);

// Resizes the layer's drawable. Called on framebuffer resize.
void set_layer_drawable_size(void *metal_layer, int width, int height);

// Enables/disables waiting for vblank on present (the CAMetalLayer equivalent of
// glfwSwapInterval).
void set_layer_display_sync(void *metal_layer, bool enabled);

// Allows or forbids using the drawable as anything other than a render target.
//
// framebufferOnly = YES lets the GPU keep the drawable losslessly compressed for
// the whole frame, which is bandwidth the tile memory does not have to spend.
// It has to be turned off before the drawable can be a blit source, so this is
// flipped once, on demand, when a screenshot is first requested.
void set_layer_framebuffer_only(void *metal_layer, bool framebuffer_only);

// Switches the layer between ordinary 8-bit output and extended dynamic range.
//
// EDR means three things at once, and all three have to move together:
//
//   * the drawable becomes RGBA16Float, so a value can exceed 1.0 at all;
//   * wantsExtendedDynamicRangeContent tells the compositor to honour those
//     values instead of clipping them at SDR white;
//   * the colour space becomes extendedLinearDisplayP3, where 1.0 IS SDR white
//     and the numbers above it are real luminance headroom.
//
// That last one also fixes, for this path only, the "sampled once at attach"
// problem the ordinary path has: extendedLinearDisplayP3 is an absolute space,
// so the compositor converts it correctly for whichever display the window is
// on. Dragging between screens needs no re-tagging.
//
// Returns true if the layer is now in the requested state.
bool set_layer_edr(void *metal_layer, bool enabled);

// How much brighter than SDR white this window's display can currently go.
//
// 1.0 on an SDR display, and on an XDR panel anything from about 2 to 16
// depending on the reference preset and the current SDR brightness. macOS moves
// it at runtime -- brightness changes, thermal pressure -- so this is a poll,
// not a constant, and reading it every frame is the intended usage.
//
// Reads the screen the window is currently on, so it also tracks a drag from
// one display to another.
double layer_edr_headroom(GLFWwindow *window);

// Writes one line per attached display: its name, whether it can do EDR, and
// its headroom right now. Called once when EDR output is turned on, because the
// first question on seeing a headroom of 1.0 is always "is that this screen, or
// is it broken?" -- and the answer is usually that the window opened on the
// other display.
//
// The callback keeps this file free of PSILog, which is C++.
void log_edr_displays(void (*log_line)(const char *name, bool capable,
                                       double headroom, bool is_current),
                      GLFWwindow *window);

} // namespace PSIMetal
