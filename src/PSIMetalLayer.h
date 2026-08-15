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

} // namespace PSIMetal
