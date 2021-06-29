#include "PSITextRenderer.h"

// PSIFontAtlas implementation.
GLboolean PSIFontAtlas::init() {
	assert(_font_path.size() > 0);
	assert(_charset.size() > 0);
	assert(_size.x > 0 && _size.y > 0);
	assert(_font_size > 0);

	// Load font from the path.
	const char *path_str = _font_path.c_str();
	psilog(PSILog::LOAD, "Loading font from path %s", path_str);

	_atlas = create_atlas(_size.x, _size.y);
	if (_atlas != nullptr) {
		texture_font_t *font = texture_font_new_from_file(_atlas, _font_size, path_str);
		if (font != nullptr) {
			texture_font_load_glyphs(font, _charset.c_str());
			_font = font;
		} else {
			psilog_err("Failed loading font from path '%s'", path_str);
			return false;
		}
	} else {
		psilog_err("Failed creating font atlas for font path '%s'", path_str);
		return false;
	}

	return true;
}

texture_atlas_t *PSIFontAtlas::create_atlas(GLsizei width, GLsizei height) {
	texture_atlas_t *atlas = texture_atlas_new(width, height, 3);
	if (atlas == nullptr) {
		psilog_err("Failed allocating texture atlas");
		return nullptr;
	}

	return atlas;
}

// PSITextRenderer implementation.
GLboolean PSITextRenderer::init() {
	assert(get_shader() != nullptr);
	assert(_font_atlas != nullptr);

	update_mesh();

	// Create our texture for storing the font atlas.
	GLTextureSharedPtr texture = PSIGLTexture::create();
	assert(texture != nullptr);

	// Assign our texture for the font atlas we are using
	GLuint atlas_id = texture->gen_texture_id(PSIGLTexture::TexType::TEX_2D);
	texture->bind();
		texture->set_size(_font_atlas->get_size());
		texture->set_sample_mode(PSIGLTexture::TexSampleMode::CLAMP | PSIGLTexture::TexSampleMode::LINEAR);
		texture->set_format(PSIGLTexture::TexFormat::RGB);
		texture->set_data(_font_atlas->get_atlas()->data);
		_font_atlas->set_atlas_texture_id(atlas_id);
	texture->unbind();

	get_material()->set_texture(texture);

	// Scale down to same distance as other engine is using.
	// Approximate one meter cube equals size of dot in font or something like that :)
	glm::vec3 scaling = { 0.01f, 0.01f, 0.01f };
	get_transform().set_scaling(scaling);

	return true;
}

void PSITextRenderer::update_mesh() {
	auto shader = get_shader();
	assert(shader != nullptr);

	if (_text.size() == 0) {
		return;
	}

	// Bake our text into GPU data.
	// Load the glyphs for this text.
	unique_data geom = bake_text(_font_atlas, _text);
	auto mesh = get_gl_mesh();

	// We don't have a mesh yet, create it.
	if (mesh == nullptr) {
		geom->buffers = {
			{ 
				PSIGLMesh::BufferName::POSITION,
				GL_ARRAY_BUFFER,
				(GLsizeiptr)(geom->positions.size() * sizeof(glm::vec3)),
				GL_STATIC_DRAW,
				nullptr
			}, { 
				PSIGLMesh::BufferName::TEXCOORD,
				GL_ARRAY_BUFFER,
				(GLsizeiptr)(geom->texcoords.size() * sizeof(glm::vec2)),
				GL_STATIC_DRAW,
				nullptr
			}, { 
				PSIGLMesh::BufferName::INDEX,
				GL_ELEMENT_ARRAY_BUFFER,
				(GLsizeiptr)(geom->indexes.size() * sizeof(GLuint)),
				GL_STATIC_DRAW,
				nullptr
			},
		};

		geom->attributes = {
			{ PSIGLMesh::BufferName::POSITION, "a_position", 3, GL_FLOAT, 0, 0, 0 },
			{ PSIGLMesh::BufferName::TEXCOORD, "a_texcoord", 2, GL_FLOAT, 0, 0, 0 },
		};

		mesh = create_gl_mesh(std::move(geom));
		if (mesh != nullptr) {
			set_gl_mesh(mesh);
			psilog(PSILog::INIT, "created mesh, _text = %s, draw_count = %d", _text.c_str(), mesh->get_draw_count());
		} else {
			psilog_err("Failed creating mesh for text renderer");
		}
	} else {
		// Mesh already created, update data.
		mesh->bind_vao();

		// Upload vertex position data.
		mesh->bind_buffer(GL_ARRAY_BUFFER, PSIGLMesh::BufferName::POSITION);
		mesh->buffer_data(GL_ARRAY_BUFFER, geom->positions.size() * sizeof(glm::vec3),
						  &geom->positions[0], GL_STATIC_DRAW);

		// Texture co-ordinates.
		mesh->bind_buffer(GL_ARRAY_BUFFER, PSIGLMesh::BufferName::TEXCOORD);
		mesh->buffer_data(GL_ARRAY_BUFFER, geom->texcoords.size() * sizeof(glm::vec2),
						  &geom->texcoords[0], GL_STATIC_DRAW);

		// We have new indexes, update indexes and draw count.
		mesh->bind_buffer(GL_ELEMENT_ARRAY_BUFFER, PSIGLMesh::BufferName::INDEX);
		mesh->buffer_data(GL_ELEMENT_ARRAY_BUFFER, geom->indexes.size() * sizeof(GLuint), 
							  &geom->indexes[0], GL_STATIC_DRAW);
		mesh->set_draw_count(geom->indexes.size());

		psilog(PSILog::OPENGL, "updated mesh, _text = %s, draw_count = %d", _text.c_str(), mesh->get_draw_count());
	}
}

void PSITextRenderer::set_text(std::string text) {
	if (_text.compare(text)) {
		_text = text;
		// Text should be able to be set before initializing, font atlas would be null then.
		if (_font_atlas != nullptr) {
			update_mesh();
		}
	}
}

#define INDEXES_PER_QUAD 6

// Bake the data for text rendered with specified font, color and position.
PSITextRenderer::unique_data PSITextRenderer::bake_text(const FontAtlasSharedPtr &atlas, std::string text) {
	assert(atlas != nullptr);
	assert(text.size() > 0);

	const char *text_str = text.c_str();

	unique_data data = make_unique<PSIGeometryData>();
	if (data == nullptr) {
		return nullptr;
	}

	// Number of characters.
	GLint text_len = strlen(text_str);
	// Number of vertexes (4 vertices per char/quad).
	GLint vertex_count = text_len * 4;

	// Reserve GPU data.
	data->texcoords.reserve(vertex_count);
	data->positions.reserve(vertex_count);
	data->indexes.reserve  (text_len * INDEXES_PER_QUAD);

	// TODO: fix this when baking multiple times.
	_glyph_dimensions.reserve(text_len);

	// Store dimensions for the text.
	_dimensions.x = 0.0f;
	glm::vec2 pos = glm::vec2(0.0f, 0.0f);

	psilog(PSILog::OPENGL, "Baking text '%s' (len=%d)", _text.c_str(), text_len);

	// TODO: should sort these in reverse order ?
	const GLfloat z_offset = 0.0f;
	texture_font_t *font = atlas->get_font();

	for(int i=0; i<text_len; ++i) {
		texture_glyph_t *glyph = texture_font_get_glyph(font, &text[i]);
		if( glyph != nullptr ) {
			// Calculate glyph position from kerning.
			GLfloat kerning = 0.0f;
			if( i > 0 && _offset_glyphs == true) {
				kerning = texture_glyph_get_kerning(glyph, &text[i-1]);
				pos.x += kerning;
				_dimensions.x += kerning;
			}

			// Position in pixels.
			GLfloat x0 = pos.x + glyph->offset_x;
			GLfloat y0 = pos.y + glyph->offset_y;
			GLfloat x1 = x0    + glyph->width;
			GLfloat y1 = y0    - glyph->height;

			// Store the glyph dimensions.
			_glyph_dimensions.push_back(glm::vec2(glyph->width, glyph->height));

			// Glyph quad vertexes.
			std::vector<glm::vec3> vertexes = {
				glm::vec3(x0, y0, z_offset),
				glm::vec3(x0, y1, z_offset),
				glm::vec3(x1, y1, z_offset),
				glm::vec3(x1, y0, z_offset)
			};

			// Texture coordinates.
			// These specify where in our texture map we have the exact texture for these triangles.
			// So when we render the triangle, we are rendering the glyph from our texture map over that triangle.
			std::vector<glm::vec2> texcoords = {
				glm::vec2(glyph->s0, glyph->t0),
				glm::vec2(glyph->s0, glyph->t1),
				glm::vec2(glyph->s1, glyph->t1),
				glm::vec2(glyph->s1, glyph->t0)
			};

			// Colors.
			// Same color for each point.
			data->positions.insert(data->positions.end(), vertexes.begin(),  vertexes.end());
			data->texcoords.insert(data->texcoords.end(), texcoords.begin(), texcoords.end());

			// Indexes.
			std::array<GLuint, 6> char_quad_indexes = PSIGeometry::Quad::calc_quad_indexes(i);
			data->indexes.insert  (data->indexes.end(), char_quad_indexes.begin(), char_quad_indexes.end());
			
			// We don't want to offset the glyphs for a timer usecase where we only want to display
			// one character at a time, from an offsetted texture position.
			if (_offset_glyphs == true) {
				pos.x = pos.x + glyph->advance_x;
			}

			// Always calculate the width of the text.
			_dimensions.x = _dimensions.x + glyph->advance_x;
		}
	}

	_dimensions.y = 0.0f;

	return data;
}

void PSITextRenderer::draw(const RenderContextSharedPtr &ctx) {
	// Empty string, don't draw.
	if (_text.size() == 0) {
		return;
	}

	auto shader = get_shader();
	auto mesh = get_gl_mesh();
	auto material = get_material();
	auto texture = material->get_texture();

	// TODO: are these required ?
	assert(shader != nullptr);
	assert(mesh != nullptr);
	assert(material	!= nullptr);
	assert(texture != nullptr);

	shader->use_program();
	texture->bind();

	// Do we translate the text based on camera position ?
	GLboolean is_translated = is_translated_by_camera();
	if (is_translated == false) {
		STACK_PUSH(ctx->view);
		ctx->view.top() = ctx->camera->get_looking_at_matrix_without_translation();
	}

	STACK_PUSH(ctx->model);
		auto asset = get_render_asset();
		calc_model_view_projection(ctx, asset.transform);

		shader->set_uniform("u_diffuse", 0);
		shader->set_uniform("u_model_view_projection_matrix", get_model_view_projection_matrix());
		shader->set_uniform("u_color", material->get_color());

		// We are not offsetting or setting custom draw count, just draw text as is.
		if (_draw_offset == -1 && _draw_count == -1) {
			mesh->draw_indexed();
		} else {
			// Offset text by offset.
			// only draw count amount of characters.
			GLint char_offset = INDEXES_PER_QUAD * _draw_offset;
			GLint char_count  = INDEXES_PER_QUAD * _draw_count;

			mesh->draw_indexed(char_offset, char_count);
		}
	ctx->model.pop();

	if (is_translated == false) {
		ctx->view.pop();
	}
}
