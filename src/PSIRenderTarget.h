// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// A set of attachments a render pass can draw into, and a script can sample
// afterwards.
//
// This replaces the single offscreen texture the renderer used to own. There
// can be any number of these, they are not pinned to the swapchain's format or
// sample count, and their depth can optionally survive the pass.
//
// What a target holds, and why:
//
//   colour            always. Single-sampled and sampleable; this is what a
//                     later pass reads.
//   msaa colour       only when samples > 1. The pass renders into this and
//                     resolves into the colour texture as it ends, exactly as
//                     the drawable path does.
//   depth             always, because the engine depth-tests everything. Held
//                     memoryless -- never leaving tile memory -- unless the
//                     target was asked for sampleable depth, in which case it
//                     is real memory with StoreActionStore.
//
// The depth mode is the one place a target can cost more than it looks: see
// PSIMetalContext::transient_storage_mode() for what memoryless buys.

#pragma once

#include "PSIGlobals.h"
#include "PSIMetal.h"
#include "PSIGLTexture.h"

class PSIRenderTarget;
typedef shared_ptr<PSIRenderTarget> RenderTargetSharedPtr;

class PSIRenderTarget {
	public:
		// Colour formats a target can be allocated in.
		//
		// These are MTL::PixelFormat values, named here so scripts do not have
		// to carry Metal's enum numbering the way psi/texture.lua carries the
		// sampler flags -- that hand-mirroring already drifted out of sync and
		// two of its values collide.
		//
		// A target in any format other than the swapchain's makes every shader
		// drawn into it compile a second pipeline; PSIGLShader keys its cache on
		// the pass signature and handles that automatically.
		enum ColorFormat {
			// Matches the drawable. No extra pipeline variants.
			FORMAT_BGRA8 = 80,      // MTL::PixelFormatBGRA8Unorm
			FORMAT_RGBA8 = 70,      // MTL::PixelFormatRGBA8Unorm
			// Half float. What bloom, tone mapping and any accumulation that
			// must not band are built on.
			FORMAT_RGBA16F = 115,   // MTL::PixelFormatRGBA16Float
			FORMAT_RGBA32F = 125,   // MTL::PixelFormatRGBA32Float
		};

		// What happens to this target's depth attachment.
		enum DepthMode {
			// Depth exists for the duration of the pass and is discarded.
			// Costs no memory on an Apple GPU. The default, and what every
			// pass wanted before shadow-style techniques were expressible.
			DEPTH_TRANSIENT = 0,
			// Depth is written to real memory and can be sampled afterwards.
			// Pays for a full-size depth texture.
			DEPTH_SAMPLE = 1,
			// No depth attachment at all. For full-screen passes that only
			// ever write one layer of fragments.
			DEPTH_NONE = 2,
		};

		PSIRenderTarget() = default;
		~PSIRenderTarget();

		static RenderTargetSharedPtr create() {
			return make_shared<PSIRenderTarget>();
		}

		// Allocate the attachments. Safe to call again to resize or reformat;
		// the previous attachments are released.
		//
		// pixel_format is an MTL::PixelFormat. samples > 1 adds a multisampled
		// colour attachment that resolves into the sampleable one.
		bool init(glm::ivec2 size, GLuint pixel_format, GLint samples, GLint depth_mode);

		// The sampleable colour result. Bind this into a material.
		const GLTextureSharedPtr &get_color_texture() const { return _color; }
		// The sampleable depth result, or null unless DEPTH_SAMPLE was asked for.
		const GLTextureSharedPtr &get_depth_texture() const { return _depth; }

		glm::ivec2 get_size() const { return _size; }
		GLint get_samples() const { return _samples; }
		GLint get_depth_mode() const { return _depth_mode; }

		// What a pipeline has to be built against to draw into this target.
		PSIMetal::pass_signature signature() const;

		// Attachments, for PSIMetalContext when it opens the pass.
		MTL::Texture *color_attachment() const;
		MTL::Texture *resolve_attachment() const;
		MTL::Texture *depth_attachment() const { return _depth_attachment; }

		bool is_valid() const { return _color != nullptr; }

	private:
		// Sampleable colour; also the resolve target when multisampled.
		GLTextureSharedPtr _color;
		// Sampleable depth. Only allocated for DEPTH_SAMPLE.
		GLTextureSharedPtr _depth;

		// Multisampled colour, when samples > 1. Not sampleable.
		MTL::Texture *_msaa_color = nullptr;
		// The depth attachment actually bound. For DEPTH_SAMPLE this is
		// _depth's texture; otherwise a memoryless one owned here.
		MTL::Texture *_depth_attachment = nullptr;
		// True when _depth_attachment is ours to release rather than _depth's.
		bool _owns_depth_attachment = false;

		glm::ivec2 _size = glm::ivec2(0, 0);
		GLuint _pixel_format = 0;
		GLint _samples = 1;
		GLint _depth_mode = DEPTH_TRANSIENT;

		void release_attachments();
};
