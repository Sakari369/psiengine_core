// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// AppKit/CAMetalLayer glue. See PSIMetalLayer.h for why this file exists.
//
// Compiled without ARC (the rest of the engine is manual-retain C++), so the
// layer is retained explicitly via CFBridgingRetain-style ownership.

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

#include "PSIMetalLayer.h"

namespace PSIMetal {

void *attach_metal_layer(GLFWwindow *window, void *mtl_device, double contents_scale) {
	if (window == nullptr || mtl_device == nullptr) {
		return nullptr;
	}

	NSWindow *ns_window = glfwGetCocoaWindow(window);
	if (ns_window == nil) {
		return nullptr;
	}

	CAMetalLayer *layer = [CAMetalLayer layer];
	if (layer == nil) {
		return nullptr;
	}

	// metal-cpp objects are the Objective-C objects, so the MTL::Device* is
	// already an id<MTLDevice>.
	layer.device = (id<MTLDevice>)mtl_device;

	// BGRA8Unorm is the only format CAMetalLayer is guaranteed to support and is
	// what the drawable hands back; the render pipelines must match it.
	layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
	layer.framebufferOnly = YES;
	layer.contentsScale = contents_scale;

	// Tag the layer with the DISPLAY's colour space, not sRGB.
	//
	// The OpenGL build was never colour managed: values went straight to the
	// panel, so glClearColor(0, 1, 0, 1) produced the display's most saturated
	// green. CAMetalLayer defaults to sRGB and Core Animation then converts
	// every pixel into the display's space -- on a P3 display that turned full
	// green into (146, 248, 58), a visible mismatch against the old renderer.
	//
	// Declaring the content to already be in the display's space means no
	// conversion happens, which reproduces the OpenGL output exactly. Note this
	// is sampled once; dragging the window to a display with a different profile
	// would not re-tag it (the GL build had the same limitation, having simply
	// never converted at all).
	NSScreen *screen = ns_window.screen ?: [NSScreen mainScreen];
	if (screen != nil && screen.colorSpace.CGColorSpace != NULL) {
		layer.colorspace = screen.colorSpace.CGColorSpace;
	}

	NSView *view = [ns_window contentView];
	[view setWantsLayer:YES];
	[view setLayer:layer];

	// The view retains the layer, but the C++ side holds this pointer for the
	// process lifetime -- take our own reference rather than relying on that.
	CFRetain((__bridge CFTypeRef)layer);

	return (void *)layer;
}

void set_layer_drawable_size(void *metal_layer, int width, int height) {
	if (metal_layer == nullptr || width <= 0 || height <= 0) {
		return;
	}
	CAMetalLayer *layer = (CAMetalLayer *)metal_layer;
	layer.drawableSize = CGSizeMake((CGFloat)width, (CGFloat)height);
}

void set_layer_display_sync(void *metal_layer, bool enabled) {
	if (metal_layer == nullptr) {
		return;
	}
	CAMetalLayer *layer = (CAMetalLayer *)metal_layer;
	layer.displaySyncEnabled = enabled ? YES : NO;
}

} // namespace PSIMetal
