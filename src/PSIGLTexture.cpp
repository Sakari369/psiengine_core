#include "PSIGLTexture.h"
#include "PSIMetalContext.h"

#include "ext/stb_image.h"

#include <cstring>
#include <vector>

namespace {

GLuint next_texture_id() {
	static GLuint counter = 0;
	return ++counter;
}

} // namespace

PSIGLTexture::~PSIGLTexture() {
	if (_sampler != nullptr) {
		_sampler->release();
		_sampler = nullptr;
	}
	if (_texture != nullptr) {
		_texture->release();
		_texture = nullptr;
	}
}

GLuint PSIGLTexture::gen_texture_id(PSIGLTexture::TexType type) {
	set_target(type == TexType::TEX_CUBEMAP ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D);
	_id = next_texture_id();
	return _id;
}

GLuint PSIGLTexture::init() {
	if (_id == (GLuint)TexDefs::INVALID_TEX_ID) {
		_id = next_texture_id();
	}
	rebuild_sampler();
	return _id;
}

// Retained so any caller still asking for GL format info gets something
// coherent. The Metal path picks MTLPixelFormat directly in create_texture().
PSIGLTexture::TexFormatInfo PSIGLTexture::get_format_info(GLint format_flags) {
	TexFormatInfo info;
	info.type = GL_UNSIGNED_BYTE;

	if ((format_flags & TexFormat::TYPE_MASK) == TexFormat::RGB) {
		info.format = GL_RGB;
		info.internal_format = GL_RGB;
	} else {
		info.format = GL_RGBA;
		info.internal_format = GL_RGBA;
	}

	return info;
}

bool PSIGLTexture::create_texture(GLint width, GLint height, GLuint face_count) {
	if (PSI_G::metal_ctx == nullptr || PSI_G::metal_ctx->device() == nullptr) {
		psilog_err("No Metal device when creating texture");
		return false;
	}
	if (width <= 0 || height <= 0) {
		return false;
	}

	if (_texture != nullptr) {
		_texture->release();
		_texture = nullptr;
	}

	MTL::TextureDescriptor *desc = MTL::TextureDescriptor::alloc()->init();

	if (face_count == 6) {
		desc->setTextureType(MTL::TextureTypeCube);
	} else {
		desc->setTextureType(MTL::TextureType2D);
	}

	// Everything is expanded to 4 channels on upload, so one format covers both
	// the 3- and 4-channel source images.
	desc->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
	desc->setWidth((NS::UInteger)width);
	desc->setHeight((NS::UInteger)height);
	desc->setUsage(MTL::TextureUsageShaderRead);
	desc->setStorageMode(MTL::StorageModeShared);

	// Mip level count must be declared up front, unlike glGenerateMipmap.
	NS::UInteger levels = 1;
	GLint dim = (width > height) ? width : height;
	while (dim > 1) {
		dim >>= 1;
		levels++;
	}
	desc->setMipmapLevelCount(levels);

	_texture = PSI_G::metal_ctx->device()->newTexture(desc);
	desc->release();

	if (_texture == nullptr) {
		psilog_err("Failed creating %dx%d texture", width, height);
		return false;
	}

	set_size(glm::vec2(width, height));

	return true;
}

bool PSIGLTexture::create_render_target(GLint width, GLint height) {
	return create_render_target(width, height,
	                            (GLuint)PSI_G::metal_ctx->color_format(), 1);
}

bool PSIGLTexture::create_render_target(GLint width, GLint height,
                                        GLuint pixel_format, GLint samples) {
	if (PSI_G::metal_ctx == nullptr || PSI_G::metal_ctx->device() == nullptr) {
		psilog_err("No Metal device when creating render target");
		return false;
	}
	if (width <= 0 || height <= 0) {
		return false;
	}
	if (samples < 1) {
		samples = 1;
	}

	if (_texture != nullptr) {
		_texture->release();
		_texture = nullptr;
	}

	const MTL::PixelFormat format = (MTL::PixelFormat)pixel_format;
	const bool multisampled = samples > 1;
	const bool is_depth = (format == MTL::PixelFormatDepth32Float ||
	                       format == MTL::PixelFormatDepth16Unorm);

	MTL::TextureDescriptor *desc = MTL::TextureDescriptor::alloc()->init();
	desc->setTextureType(multisampled ? MTL::TextureType2DMultisample : MTL::TextureType2D);
	desc->setSampleCount(static_cast<NS::UInteger>(samples));
	// Any format the device supports as a render target. Pipelines bake the
	// attachment format in, which is why offscreen targets used to be pinned to
	// the swapchain's -- PSIGLShader now keys its pipeline cache on the pass
	// signature and compiles a variant per format instead.
	desc->setPixelFormat(format);
	desc->setWidth(static_cast<NS::UInteger>(width));
	desc->setHeight(static_cast<NS::UInteger>(height));
	desc->setMipmapLevelCount(1);
	desc->setUsage(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
	// Sampleable, so it has to live in real memory -- this is the opposite of
	// the transient attachments PSIMetalContext allocates memoryless.
	desc->setStorageMode(MTL::StorageModePrivate);

	_texture = PSI_G::metal_ctx->device()->newTexture(desc);
	desc->release();

	if (_texture == nullptr) {
		psilog_err("Failed creating %dx%d render target (format %u, %d samples)",
		           width, height, pixel_format, samples);
		return false;
	}

	_id = next_texture_id();
	_target = GL_TEXTURE_2D;
	_has_mipmaps = false;
	_samples = samples;
	set_size(glm::vec2(width, height));

	// A depth texture sampled with a linear filter is not universally supported;
	// nearest is always valid and is what a depth lookup wants anyway.
	if (is_depth) {
		_sample_mode = TexSampleMode::NEAREST | TexSampleMode::CLAMP;
	}
	rebuild_sampler();

	psilog(PSILog::TEXTURE, "Created %dx%d render target (format %u, %d samples), id = %d",
	       width, height, pixel_format, samples, _id);

	return true;
}

void PSIGLTexture::upload_image(const unsigned char *pixels, GLint width, GLint height,
                                GLint channels, GLuint slice) {
	if (_texture == nullptr || pixels == nullptr) {
		return;
	}

	// Expand to RGBA. Deliberately NOT flipped vertically.
	//
	// The usual "OpenGL and Metal disagree about the texture origin" advice does
	// not apply to this port. Both APIs map texture coordinate 0 to the FIRST row
	// of uploaded data -- OpenGL calls that row the bottom-left origin and Metal
	// calls it the top-left, but the array-index-to-coordinate mapping is the
	// same. With identical bytes and identical UVs, both sample the same texel.
	//
	// The engine never called stbi_set_flip_vertically_on_load(), so uploading
	// stb's rows in order reproduces the OpenGL build exactly. Flipping here
	// would introduce the very difference it looks like it is preventing.
	const GLint dst_channels = 4;
	std::vector<unsigned char> rgba((size_t)width * height * dst_channels);

	for (GLint y = 0; y < height; y++) {
		const unsigned char *src_row = pixels + (size_t)y * width * channels;
		unsigned char *dst_row = rgba.data() + (size_t)y * width * dst_channels;

		for (GLint x = 0; x < width; x++) {
			const unsigned char *s = src_row + (size_t)x * channels;
			unsigned char *d = dst_row + (size_t)x * dst_channels;

			d[0] = s[0];
			d[1] = (channels > 1) ? s[1] : s[0];
			d[2] = (channels > 2) ? s[2] : s[0];
			d[3] = (channels > 3) ? s[3] : 255;
		}
	}

	MTL::Region region = MTL::Region::Make2D(0, 0, (NS::UInteger)width, (NS::UInteger)height);
	_texture->replaceRegion(region,
	                        0,
	                        (NS::UInteger)slice,
	                        rgba.data(),
	                        (NS::UInteger)width * dst_channels,
	                        0);
}

void PSIGLTexture::set_data(const GLvoid *data) {
	if (data == nullptr) {
		return;
	}

	GLint width = (GLint)_size.x;
	GLint height = (GLint)_size.y;
	if (width <= 0 || height <= 0) {
		psilog_err("set_data() called before the texture size was set");
		return;
	}

	// PSITextRenderer uses this to push the freetype-gl glyph atlas, which is
	// RGB bytes in RAM.
	GLint channels = ((_format & TexFormat::TYPE_MASK) == TexFormat::RGB) ? 3 : 4;

	if (_texture == nullptr && !create_texture(width, height, 1)) {
		return;
	}

	upload_image(static_cast<const unsigned char *>(data), width, height, channels, 0);
	rebuild_sampler();
}

void PSIGLTexture::load_from_file(std::string path) {
	GLint width = 0;
	GLint height = 0;
	GLint channels = 0;

	psilog(PSILog::TEXTURE, "Loading '%s'", path.c_str());

	unsigned char *image = stbi_load(path.c_str(), &width, &height, &channels, STBI_default);
	if (image == nullptr) {
		psilog_err("Failed loading image from '%s'", path.c_str());
		return;
	}

	gen_texture_id(TexType::TEX_2D);
	set_format(channels == 3 ? TexFormat::RGB : TexFormat::RGBA);

	if (!create_texture(width, height, 1)) {
		stbi_image_free(image);
		return;
	}

	upload_image(image, width, height, channels, 0);
	stbi_image_free(image);

	psilog(PSILog::TEXTURE, "Loaded '%s' [%dx%d c=%d], id = %d",
	       path.c_str(), width, height, channels, _id);

	set_sample_mode(TexSampleMode::CLAMP | TexSampleMode::ANISOTROPIC);
	gen_mipmaps(true);
}

void PSIGLTexture::load_cube_map(std::vector<std::string> texture_paths) {
	gen_texture_id(TexType::TEX_CUBEMAP);
	set_format(TexFormat::RGB);

	GLint width = 0;
	GLint height = 0;
	GLint channels = 0;

	for (GLuint i = 0; i < texture_paths.size() && i < 6; i++) {
		psilog(PSILog::TEXTURE, "Loading '%s'", texture_paths[i].c_str());

		unsigned char *image = stbi_load(texture_paths[i].c_str(), &width, &height,
		                                 &channels, STBI_rgb);
		if (image == nullptr) {
			psilog_err("Failed loading image from '%s'", texture_paths[i].c_str());
			continue;
		}

		// All six faces share one cube texture, allocated from the first face.
		if (_texture == nullptr && !create_texture(width, height, 6)) {
			stbi_image_free(image);
			return;
		}

		upload_image(image, width, height, 3, i);
		stbi_image_free(image);

		psilog(PSILog::TEXTURE, "Loaded '%s' [%dx%d c=%d]",
		       texture_paths[i].c_str(), width, height, channels);
	}

	set_sample_mode(TexSampleMode::CLAMP | TexSampleMode::LINEAR_MIPMAP);
	gen_mipmaps(true);

	psilog(PSILog::TEXTURE, "Loaded cubemap texture with id = %d", _id);
}

void PSIGLTexture::set_sample_mode(GLint sample_mode) {
	_sample_mode = sample_mode;
	rebuild_sampler();
}

void PSIGLTexture::rebuild_sampler() {
	if (PSI_G::metal_ctx == nullptr || PSI_G::metal_ctx->device() == nullptr) {
		return;
	}

	if (_sampler != nullptr) {
		_sampler->release();
		_sampler = nullptr;
	}

	MTL::SamplerDescriptor *desc = MTL::SamplerDescriptor::alloc()->init();

	// Same mask arithmetic as the GL version, so the flag values that
	// assets/scripts/psi/texture.lua mirrors keep behaving identically.
	switch (_sample_mode & TexSampleMode::FILTER_MASK) {
	default:
	case TexSampleMode::LINEAR:
		desc->setMinFilter(MTL::SamplerMinMagFilterLinear);
		desc->setMagFilter(MTL::SamplerMinMagFilterLinear);
		desc->setMipFilter(MTL::SamplerMipFilterNotMipmapped);
		break;

	case TexSampleMode::LINEAR_MIPMAP:
		desc->setMinFilter(MTL::SamplerMinMagFilterLinear);
		desc->setMagFilter(MTL::SamplerMinMagFilterLinear);
		desc->setMipFilter(_has_mipmaps ? MTL::SamplerMipFilterLinear
		                                : MTL::SamplerMipFilterNotMipmapped);
		desc->setMaxAnisotropy(1);
		break;

	case TexSampleMode::NEAREST:
		desc->setMinFilter(MTL::SamplerMinMagFilterNearest);
		desc->setMagFilter(MTL::SamplerMinMagFilterNearest);
		desc->setMipFilter(MTL::SamplerMipFilterNotMipmapped);
		desc->setMaxAnisotropy(1);
		break;

	case TexSampleMode::ANISOTROPIC:
		desc->setMinFilter(MTL::SamplerMinMagFilterLinear);
		desc->setMagFilter(MTL::SamplerMinMagFilterLinear);
		desc->setMipFilter(_has_mipmaps ? MTL::SamplerMipFilterLinear
		                                : MTL::SamplerMipFilterNotMipmapped);
		desc->setMaxAnisotropy(TexDefs::DEF_MAX_ANISOTROPY);
		break;
	}

	MTL::SamplerAddressMode address = MTL::SamplerAddressModeRepeat;
	switch (_sample_mode & TexSampleMode::ADDRESS_MASK) {
	default:
	case TexSampleMode::REPEAT:
		address = MTL::SamplerAddressModeRepeat;
		break;

	case TexSampleMode::CLAMP:
		address = MTL::SamplerAddressModeClampToEdge;
		break;

	case TexSampleMode::CLAMP_BORDER:
		address = MTL::SamplerAddressModeClampToBorderColor;
		desc->setBorderColor(MTL::SamplerBorderColorOpaqueBlack);
		break;
	}

	desc->setSAddressMode(address);
	desc->setTAddressMode(address);
	desc->setRAddressMode(address);

	_sampler = PSI_G::metal_ctx->device()->newSamplerState(desc);
	desc->release();
}

void PSIGLTexture::gen_mipmaps(GLboolean generate) {
	if (!generate || _texture == nullptr || PSI_G::metal_ctx == nullptr) {
		return;
	}
	if (_texture->mipmapLevelCount() <= 1) {
		return;
	}

	// glGenerateMipmap had no explicit command buffer; Metal needs a blit pass.
	// This runs at load time, outside the frame's command buffer, so it gets its
	// own and waits -- the texture must be complete before first use.
	MTL::CommandBuffer *cmd = PSI_G::metal_ctx->queue()->commandBuffer();
	if (cmd == nullptr) {
		return;
	}

	MTL::BlitCommandEncoder *blit = cmd->blitCommandEncoder();
	if (blit == nullptr) {
		return;
	}

	blit->generateMipmaps(_texture);
	blit->endEncoding();
	cmd->commit();
	cmd->waitUntilCompleted();

	_has_mipmaps = true;

	// The mip filter depends on mipmaps existing, so rebuild with that known.
	rebuild_sampler();

	psilog(PSILog::TEXTURE, "Generated %lu mipmap levels",
	       (unsigned long)_texture->mipmapLevelCount());
}

void PSIGLTexture::bind() {
	if (PSI_G::metal_ctx == nullptr || _texture == nullptr) {
		return;
	}

	MTL::RenderCommandEncoder *encoder = PSI_G::metal_ctx->encoder();
	if (encoder == nullptr) {
		// Called during asset loading, outside a frame. The GL version bound to
		// global state here; there is nothing to do until a draw.
		return;
	}

	if (_sampler == nullptr) {
		rebuild_sampler();
	}

	// Slot 0 matches [[texture(0)]] / [[sampler(0)]] in the shaders and the
	// "u_diffuse" = 0 the draw path sets.
	encoder->setFragmentTexture(_texture, 0);
	if (_sampler != nullptr) {
		encoder->setFragmentSamplerState(_sampler, 0);
	}
}

void PSIGLTexture::unbind() {
	// No global binding point to clear; the next draw sets what it needs.
}
