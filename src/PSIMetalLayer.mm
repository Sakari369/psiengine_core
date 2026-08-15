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

	// Pin the colour space to sRGB. Without this the layer inherits the
	// display's (P3 on most modern Macs), so shader output gets re-mapped on the
	// way to screen and colours no longer match the OpenGL build -- which makes
	// side-by-side visual comparison during the port useless.
	CGColorSpaceRef srgb = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
	if (srgb != NULL) {
		layer.colorspace = srgb;
		CGColorSpaceRelease(srgb);
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
