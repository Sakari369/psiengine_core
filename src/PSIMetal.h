// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// All Metal related includes.
//
// Replaces the OpenGL backend. metal-cpp is Apple's official header-only C++
// binding, vendored under src/ext/metal-cpp. The matching implementation
// translation unit is src/ext/metal_cpp_impl.cpp -- exactly one TU in the whole
// program may define the *_PRIVATE_IMPLEMENTATION macros.

#pragma once

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include "PSITypes.h"

namespace PSIMetal {

// Buffer index convention.
//
// Metal puts vertex attribute buffers and constant buffers in one 31-slot
// namespace. Slots 0-6 mirror PSIGLShader::AttribLocation / PSIGLMesh::BufferName
// (POSITION, COLOR, TEXCOORD, NORMAL, TANGENT, SEGMENT, ANGLE), so a mesh binds
// its per-attribute buffers at the same index the shader declares
// [[attribute(n)]] with. Uniform blocks start at 16 to stay clear of them.
enum BufferIndex {
	BUFFER_ATTRIB_FIRST = 0,
	BUFFER_ATTRIB_LAST  = 6,

	BUFFER_UNIFORMS_VERTEX   = 16,
	BUFFER_UNIFORMS_FRAGMENT = 17,
	// Per-instance data for the geometry-shader replacement paths (Poly, points).
	BUFFER_INSTANCE_DATA     = 18,
};

// Number of frames the CPU may run ahead of the GPU. Guarded by a counting
// semaphore in PSIMetalContext; sizes the uniform ring buffer.
constexpr int MAX_FRAMES_IN_FLIGHT = 3;

// Everything about a render pass that a pipeline has to be built against.
//
// Metal bakes the attachment formats and the sample count into every
// MTLRenderPipelineState, so a shader cannot be used in a pass whose targets
// differ from the ones its pipeline was compiled for. That is why offscreen
// targets were previously required to match the swapchain exactly.
//
// PSIGLShader keys its pipeline cache on this, so a pass can render into any
// format it likes and the shader compiles a variant for it on first use.
struct pass_signature {
	MTL::PixelFormat color_format = MTL::PixelFormatBGRA8Unorm;
	MTL::PixelFormat depth_format = MTL::PixelFormatDepth32Float;
	uint32_t sample_count = 1;

	bool operator==(const pass_signature &rhs) const {
		return color_format == rhs.color_format &&
		       depth_format == rhs.depth_format &&
		       sample_count == rhs.sample_count;
	}
	bool operator!=(const pass_signature &rhs) const {
		return !(*this == rhs);
	}
};

} // namespace PSIMetal
