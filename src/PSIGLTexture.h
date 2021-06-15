// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// OpenGL texture interface.

#pragma once

#include <stack>
#include <functional>

#include "PSIGlobals.h"
#include "PSIMath.h"
#include "PSIOpenGL.h"
#include "PSITypes.h"

class PSIGLTexture {
	public:

	// OpenGL Texture formats.
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

	// OpenGL Texture sample modes.
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

	// Format info for generated texture.
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
	~PSIGLTexture() = default;

	// Initialize a default texture.
	GLuint init();

	// Bind this texture to our texture id.
	void bind() {
		glBindTexture(_target, _id);
		//psilog(PSILog::FREQ, "Bound texture with target = %d, id = %d", _target, _id);
	}
	// Unbind this texture.
	void unbind() {
		glBindTexture(_target, 0);
	}

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
	// Generate texture id and set active.
	GLuint gen_texture_id(PSIGLTexture::TexType type);

	private:
	// Generates a default 2d texture with sane defaults.
	GLuint gen_2d_texture(GLint format, GLint width, GLint height);
	// Set texture mipmaps. Mipmaps are generated if generate set to true.
	void gen_mipmaps(GLboolean generate);
	// Get the OpenGL texture format information from the format flags.
	PSIGLTexture::TexFormatInfo get_format_info(GLint format_flags);

	// OpenGL texture id.
	GLuint _id = TexDefs::INVALID_TEX_ID;
	// OpenGL texture format.
	GLint _format = TexFormat::RGBA;
	// If samples > 1, generate multisample texture.
	GLint _samples = 1;
	// Target which texture is bound to.
	GLenum _target = GL_TEXTURE_2D;
	// Dimensions in pixels.
	glm::vec2 _size = { 0.0f, 0.0f };
};
