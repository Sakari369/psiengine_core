#include "ext/stb_image.h"

#include "PSIGLTexture.h"
#include "PSIGLUtils.h"

GLuint PSIGLTexture::init() {
	assert(_size.x > 0);
	assert(_size.y > 0);

	GLuint texture_id = gen_2d_texture(_format, _size.x, _size.y);
	set_id(texture_id);

	return texture_id;
}

GLuint PSIGLTexture::gen_texture_id(PSIGLTexture::TexType type) {
	GLenum target = GL_TEXTURE_2D;
	if (type == TexType::TEX_2D && get_samples() > 1) {
		target = GL_TEXTURE_2D_MULTISAMPLE;
	} else if (type == TexType::TEX_CUBEMAP) {
		target = GL_TEXTURE_CUBE_MAP;
	}

	// Set our target.
	set_target(target);

	// Allocate texture id.
	GLuint id;
	glGenTextures(1, &id);
	set_id(id);

	// Set.
	glActiveTexture(GL_TEXTURE0);

	check_gl_error();

	return id;
}

PSIGLTexture::TexFormatInfo PSIGLTexture::get_format_info(GLint format_flags) {
	// Default type.
	PSIGLTexture::TexFormatInfo info = { GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE };

	// Figure out internal texture format.
	switch(format_flags & TexFormat::TYPE_MASK)
	{
	case TexFormat::RGB:
		info.format = GL_RGB;
		break;

	case TexFormat::RGBA:
		info.format = GL_RGBA; 
		break;

	case TexFormat::R:
		info.format = GL_RED; 
		break;

	case TexFormat::DEPTH: 
		info.format = GL_DEPTH_COMPONENT; 
		info.type = GL_FLOAT; 
		break;

	case TexFormat::DXT1:
		info.format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		break;
	case TexFormat::DXT3:
		info.format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		break;
	case TexFormat::DXT5:
		info.format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		break;

	default:
		psilog(PSILog::TEXTURE, "Warning: invalid format flags passed!");
		break;
	}

	GLboolean is_srgb = false;
	if (   (format_flags & TexFormat::TYPE_MASK) == TexFormat::RGBA
	    && (format_flags & TexFormat::SRGB)     != 0) {
		is_srgb = true;
	}

	GLboolean is_depth = false;
	if ( (format_flags & TexFormat::DEPTH) != 0) {
		is_depth = true;
	}

	info.internal_format = info.format;
	if (is_srgb) {
		info.internal_format = GL_SRGB8_ALPHA8;
	} else if (is_depth) {
		info.internal_format = GL_DEPTH_COMPONENT24;
	}

	psilog(PSILog::TEXTURE, "internal_format = %d format = %d type = %d", info.internal_format, info.format, info.type);

	return info;
}

GLuint PSIGLTexture::gen_2d_texture(GLint format, GLint width, GLint height) {
	// Generate the texture and set render target
	GLuint id = gen_texture_id(TexType::TEX_2D);
	bind();

	TexFormatInfo fmt = get_format_info(format);

	// Specify empty multisampled texture ?
	GLint samples = get_samples();
	GLenum target = get_target();
	if (samples > 1) {
		glTexImage2DMultisample(target, samples, fmt.internal_format, width, height, GL_TRUE);
	} else {
		glTexImage2D(target, 0, fmt.internal_format, width, height, 0, fmt.format, fmt.type, NULL);
	}

	set_sample_mode(PSIGLTexture::TexSampleMode::REPEAT | PSIGLTexture::TexSampleMode::LINEAR_MIPMAP);

	GLboolean generate_mipmaps = (GLboolean)(format & TexFormat::GEN_MIPMAPS);
	gen_mipmaps(generate_mipmaps);

	psilog(PSILog::OPENGL, "Generated texture, id=%d (%dx%d), %d sample(s)", id, width, height, samples);

	unbind();

	return id;
}

void PSIGLTexture::set_data(const GLvoid *data) {
	TexFormatInfo fmt = get_format_info(_format);
	glTexImage2D(get_target(), 0, fmt.internal_format, _size.x, _size.y, 0, fmt.format, fmt.type, data);
}

void PSIGLTexture::set_sample_mode(GLint sample_mode) {
	GLenum target = get_target();

	switch (sample_mode & TexSampleMode::FILTER_MASK) {
	default:
	case TexSampleMode::LINEAR:
		glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		break;

	case TexSampleMode::LINEAR_MIPMAP:
		glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		if(GL_EXT_texture_filter_anisotropic) {
			glTexParameteri(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
		}
		break;

	case TexSampleMode::NEAREST:
		glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		if(GL_EXT_texture_filter_anisotropic) {
			glTexParameteri(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
		}
		break;

	case TexSampleMode::ANISOTROPIC:
		glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		if(GL_EXT_texture_filter_anisotropic) {
			glTexParameteri(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, TexDefs::DEF_MAX_ANISOTROPY);
		}
		break;
	}

	switch (sample_mode & TexSampleMode::ADDRESS_MASK) {
	default:
	case TexSampleMode::REPEAT:
		glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_REPEAT);
		if (target == GL_TEXTURE_CUBE_MAP) {
			glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_REPEAT);
		}

		break;

	case TexSampleMode::CLAMP:
		glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		if (target == GL_TEXTURE_CUBE_MAP) {
			glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		}
		break;

	case TexSampleMode::CLAMP_BORDER:
		glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		if (target == GL_TEXTURE_CUBE_MAP) {
			glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
		}
		break;
	}

	check_gl_error();
}

void PSIGLTexture::gen_mipmaps(GLboolean generate) {
	GLenum target = get_target();
	GLint mipmap_count = 0;
	if (generate == true) {
		mipmap_count = TexDefs::DEF_MIPMAP_COUNT;
	}

	// Have to set these parameters anyway.
	glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, mipmap_count);

	// Generate mipmaps.
	if (mipmap_count > 0) {
		glGenerateMipmap(target);
		psilog(PSILog::TEXTURE, "Generated %d mipmap levels", mipmap_count);
	}
}

void PSIGLTexture::load_from_file(std::string path) {
	// Width and height from loaded image.
	GLint width;
	GLint height;
	// Number of color channels from loaded image.
	GLint channels;

	psilog(PSILog::TEXTURE, "Loading '%s'", path.c_str());

	// Load the image data.
	unsigned char *image = stbi_load(path.c_str(), &width, &height, &channels, STBI_default);

	// Set the texture image data
	if (image != nullptr) {
		GLuint id = gen_texture_id(TexType::TEX_2D);
		bind();
		if (id != TexDefs::INVALID_TEX_ID) {
			// Does the image loaded have RGB or RGBA data in it ?
			// The generated texture depends on the data format.
			TexFormat fmt;
			if (channels == 3) {
				fmt = TexFormat::RGB;
			} else {
				fmt = TexFormat::RGBA;
			}
			// Get more detailed info based on channel format.
			TexFormatInfo fmt_info = get_format_info(fmt);
			GLenum target = get_target();
			// Generate 2D texture.
			glTexImage2D(target, 0, fmt_info.internal_format, width, height, 0, fmt_info.format, fmt_info.type, image);

			psilog(PSILog::TEXTURE, "Loaded '%s' [%dx%d c=%d], id = %d", path.c_str(), width, height, channels, id);
		} else {
			stbi_image_free(image);
			psilog(PSILog::TEXTURE, "Failed allocating texture for file '%s'", path.c_str());
			return;
		}
	} else {
		psilog(PSILog::TEXTURE, "Failed loading image from '%s'", path.c_str());
		return;
	}

	stbi_image_free(image);
	set_sample_mode(PSIGLTexture::TexSampleMode::CLAMP | PSIGLTexture::TexSampleMode::ANISOTROPIC);
	gen_mipmaps(true);
	unbind();
}

void PSIGLTexture::load_cube_map(std::vector<std::string> texture_paths) {
	GLuint id = gen_texture_id(TexType::TEX_CUBEMAP);
	bind();

	// Width and height from loaded image.
	GLint width;
	GLint height;
	// Number of color channels from loaded image.
	GLint channels;

	// Load all the cube map textures.
	for(GLuint i=0; i<texture_paths.size(); i++) {
		psilog(PSILog::TEXTURE, "Loading '%s'", texture_paths[i].c_str());
		unsigned char *image = stbi_load(texture_paths[i].c_str(), &width, &height, &channels, STBI_rgb);

		if (image != nullptr) {
			// For cubemap textures always use GL_RGB.
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
			stbi_image_free(image);
			psilog(PSILog::TEXTURE, "Loaded '%s' [%dx%d c=%d]", texture_paths[i].c_str(), width, height, channels);
		} else {
			psilog_err("Failed loading image from '%s'", texture_paths[i].c_str());
		}
	}

	set_sample_mode(PSIGLTexture::TexSampleMode::CLAMP | PSIGLTexture::TexSampleMode::LINEAR_MIPMAP);
	gen_mipmaps(true);
	unbind();

	psilog(PSILog::TEXTURE, "Loaded cubemap texture with id = %d", id);
}  
