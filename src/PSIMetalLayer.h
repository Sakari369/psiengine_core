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

// How the drawable's numbers are meant to be read.
//
// Every shader in this tree writes linear light values. What differs between
// these is what the compositor is told about them.
//
//   LEGACY  8-bit, tagged with the display's own space, so Core Animation
//           passes the values through unconverted and the panel applies its own
//           ~2.2 gamma to numbers that were never encoded. Displayed luminance
//           ends up proportional to L^2.2 rather than L. This is wrong, and it
//           is what the OpenGL build did, so it is the default until each demo
//           has been retuned off it.
//
//   SRGB    8-bit sRGB, so the GPU encodes on write -- in the ROP, for free --
//           and blending and MSAA resolve happen in linear and are written back
//           correctly. Tagged sRGB, which is what the demos' colours and
//           textures are authored in, so Core Animation converts properly for a
//           P3 panel instead of oversaturating.
//
//   EDR     RGBA16Float in extendedLinearDisplayP3: linear all the way, and
//           values above 1.0 become headroom above SDR white. Already colour
//           managed by construction, which is why the EDR path looks brighter
//           than the LEGACY one at the same headroom.
enum LayerOutput {
	LAYER_OUTPUT_LEGACY = 0,
	LAYER_OUTPUT_SRGB   = 1,
	LAYER_OUTPUT_EDR    = 2,
};

// Returns true if the layer is in the requested mode afterwards.
bool set_layer_output(void *metal_layer, int mode);

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
