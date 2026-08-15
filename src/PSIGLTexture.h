// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Texture interface. Metal backend.
//
// Public API unchanged: PSIGLTexture is bound to Lua (LuaAPI.cpp:445) and
// scripts drive it with load_from_file(), bind(), set_sample_mode() and
// unbind() -- see assets/scripts/game/area.lua:27-29.
//
// The TexFormat / TexSampleMode flag values are part of that contract too:
// assets/scripts/psi/texture.lua mirrors them by hand, so the numbers must not
// change even though they are no longer GL enums.

#pragma once

#include <stack>
#include <functional>

#include "PSIGlobals.h"
#include "PSIMath.h"
#include "PSIOpenGL.h"
#include "PSITypes.h"

class PSIGLTexture;
typedef shared_ptr<PSIGLTexture> GLTextureSharedPtr;

class PSIGLTexture {
	public:

	// Texture formats. Engine-specific flags, not GL enums.
	enum TexFormat {
		RGB		= 0x100,
		RGBA            = 0x200,
		R               = 0x400,
		A		= 0x800,
		BGRA            = 0x1000,
		DXT1            = 0x1100,
		DXT3            = 0x1200,
		DXT5            = 0x1300,
		DEPTH           = 0x8000,
		TYPE_MASK       = 0xff00,
		COMPRESSED      = 0x1000,
		SAMPLES_MASK    = 0x00ff,
		RENDERTARGET    = 0x10000,
		SAMPLEDEPTH	= 0x20000,
		GEN_MIPMAPS     = 0x40000,
		SRGB		= 0x80000,
	};

	// Texture sample modes. Mirrored in assets/scripts/psi/texture.lua -- keep
	// the values.
	enum TexSampleMode {
		LINEAR       	= 0x000,
		LINEAR_MIPMAP  	= 0x001,
		NEAREST      	= 0x010,
		ANISOTROPIC  	= 0x011,
		FILTER_MASK   	= 0x111,
		REPEAT       	= 0x000,
		CLAMP        	= 0x1000,
		CLAMP_BORDER  	= 0x1001, // If unsupported Clamp is used instead.
		ADDRESS_MASK  	= 0x1111,
		COUNT        	= 13,
	};

	enum TexType {
		TEX_2D		= 0,
		TEX_CUBEMAP	= 1
	};

	// Format info for generated texture. Retained for API compatibility;
	// resolve_pixel_format() is what the Metal path actually uses.
	struct TexFormatInfo {
		GLenum format;
		GLenum internal_format;
		GLenum type;
	};

	// General texture definitions.
	enum TexDefs {
		INVALID_TEX_ID	= -1,
		DEF_MIPMAP_COUNT = 16,
		DEF_MAX_ANISOTROPY = 16
	};

	PSIGLTexture() = default;
	~PSIGLTexture();

	static GLTextureSharedPtr create() {
		return make_shared<PSIGLTexture>();
	}

	// Initialize a default texture.
	GLuint init();

	// Bind this texture for the next draw.
	//
	// OpenGL bound to a global slot that persisted until changed; Metal sets
	// texture and sampler on the render encoder. Outside a frame (during asset
	// loading, where the GL code also called bind()) this is a no-op.
	void bind();
	void unbind();

	void set_id(GLuint id) { _id = id; }
	GLuint get_id() { return _id; }

	void set_size(glm::vec2 size) { _size = size; }
	glm::vec2 get_size() { return _size; }

	void set_format(GLint format) { _format = format; }
	GLint get_format() { return _format; }

	void set_target(GLenum target) { _target = target; }
	GLenum get_target() { return _target; }

	void set_samples(GLint samples) {
		_samples = (samples < 1) ? 1 : samples;
	}
	GLint get_samples() {
		return _samples;
	}

	void set_sample_mode(GLint sample_mode);
	void set_data(const GLvoid *data);

	// Load texture image from file and generate texture.
	void load_from_file(std::string path);
	// Load all the faces of a cube map and generate cubemap texture.
	void load_cube_map(std::vector<std::string> texture_paths);
	// Kept for API compatibility; Metal allocates on upload.
	GLuint gen_texture_id(PSIGLTexture::TexType type);

	MTL::Texture *get_metal_texture() const { return _texture; }
	MTL::SamplerState *get_sampler() const { return _sampler; }

	private:
	// Allocate the MTLTexture for the current size/format/target.
	bool create_texture(GLint width, GLint height, GLuint face_count);
	// Upload one image, flipping rows (see the note in the .cpp).
	void upload_image(const unsigned char *pixels, GLint width, GLint height,
	                  GLint channels, GLuint slice);
	// Build the sampler state from the current sample mode.
	void rebuild_sampler();
	// Generate mipmaps with a blit encoder.
	void gen_mipmaps(GLboolean generate);
	// Retained for API compatibility.
	PSIGLTexture::TexFormatInfo get_format_info(GLint format_flags);

	MTL::Texture *_texture = nullptr;
	MTL::SamplerState *_sampler = nullptr;

	// Handle for logging and for the Lua-visible get_id().
	GLuint _id = TexDefs::INVALID_TEX_ID;
	// Texture format flags.
	GLint _format = TexFormat::RGBA;
	// If samples > 1, generate multisample texture.
	GLint _samples = 1;
	// GL_TEXTURE_2D or GL_TEXTURE_CUBE_MAP.
	GLenum _target = GL_TEXTURE_2D;
	// Dimensions in pixels.
	glm::vec2 _size = { 0.0f, 0.0f };

	// Current sample mode, so the sampler can be rebuilt when it changes.
	GLint _sample_mode = TexSampleMode::LINEAR | TexSampleMode::REPEAT;
	// Whether mipmaps were generated, which decides the sampler's mip filter.
	bool _has_mipmaps = false;
};
