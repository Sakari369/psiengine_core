#include "PSIGLTFLoader.h"

#include <cstring>

namespace {

// Size in bytes of one glTF component. The values are the GL scalar type
// constants glTF 1.0 inherited.
size_t gltf_component_size(GLint component_type) {
	switch (component_type) {
	case TINYGLTF_COMPONENT_TYPE_BYTE:
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
		return 1;
	case TINYGLTF_COMPONENT_TYPE_SHORT:
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
		return 2;
	case TINYGLTF_COMPONENT_TYPE_INT:
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
	case TINYGLTF_COMPONENT_TYPE_FLOAT:
		return 4;
	default:
		return 0;
	}
}

// Copy an accessor's elements out of its buffer view into a tightly packed
// block.
//
// glTF stores attributes inside shared buffer views at arbitrary offsets and
// strides, but PSIGLMesh keeps one tightly packed buffer per attribute -- the
// layout PSIGLShader builds its vertex descriptor around. De-interleaving here
// at load time is far simpler than teaching the shared vertex descriptor about
// per-attribute offsets and strides, and it costs one copy per model.
bool gltf_pack_accessor(const tinygltf::Scene &scene,
                        const tinygltf::Accessor &accessor,
                        size_t element_size,
                        std::vector<uint8_t> *out) {
	auto view_it = scene.bufferViews.find(accessor.bufferView);
	if (view_it == scene.bufferViews.end()) {
		return false;
	}
	const tinygltf::BufferView &view = view_it->second;

	auto buffer_it = scene.buffers.find(view.buffer);
	if (buffer_it == scene.buffers.end()) {
		return false;
	}
	const tinygltf::Buffer &buffer = buffer_it->second;

	// A zero stride means tightly packed.
	size_t stride = (accessor.byteStride > 0) ? accessor.byteStride : element_size;
	size_t start = view.byteOffset + accessor.byteOffset;
	size_t needed = (accessor.count > 0) ? (start + (accessor.count - 1) * stride + element_size) : start;

	if (needed > buffer.data.size()) {
		psilog_err("glTF accessor runs past its buffer (%zu > %zu)", needed, buffer.data.size());
		return false;
	}

	out->resize(accessor.count * element_size);
	const uint8_t *src = buffer.data.data() + start;
	uint8_t *dst = out->data();

	for (size_t i = 0; i < accessor.count; i++) {
		std::memcpy(dst + i * element_size, src + i * stride, element_size);
	}

	return true;
}

} // namespace

GLMeshSharedPtr PSIGLTFLoader::create_gl_mesh(const ShaderSharedPtr &shader, const tinygltf::Scene &scene) {
	psilog(PSILog::OPENGL, "Creating mesh from glTF scene");

	// Create new mesh object.
	GLMeshSharedPtr mesh_obj = PSIGLMesh::create();
	mesh_obj->init();

	// Setup the meshes.
	for (const auto &node_it : scene.nodes ) {
		const tinygltf::Node &node = node_it.second;

		GLuint draw_count = 0;
		GLint draw_mode = 0;
		GLenum index_type = GL_UNSIGNED_SHORT;
		for (const auto &mesh_name : node.meshes) {
			auto mesh_it = scene.meshes.find(mesh_name);

			// Initialize buffers, indexes and vertex attributes for this mesh.
			if (mesh_it != scene.meshes.end()) {
				psilog(PSILog::OPENGL, "Setting up mesh %s", mesh_it->first.c_str());

				const tinygltf::Mesh &mesh = mesh_it->second;
				for (const auto &primitive : mesh.primitives) {
					//plog_s("primitive.indices_accessor = %s", primitive.indices.c_str());
					// Get the indexes accessor 
					auto indices_accessor_it = scene.accessors.find(primitive.indices);
      				const tinygltf::Accessor &indices_accessor = indices_accessor_it->second;

					draw_count = indices_accessor.count;
					index_type = indices_accessor.componentType;

					// Upload the index data, packed.
					size_t index_size = gltf_component_size(index_type);
					std::vector<uint8_t> indices;
					if (index_size > 0 &&
					    gltf_pack_accessor(scene, indices_accessor, index_size, &indices)) {
						mesh_obj->bind_buffer(GL_ELEMENT_ARRAY_BUFFER, PSIGLMesh::BufferName::INDEX);
						mesh_obj->buffer_data(GL_ELEMENT_ARRAY_BUFFER, indices.size(),
						                      indices.data(), GL_STATIC_DRAW);

						psilog(PSILog::OPENGL, "Uploaded %d indices (%zu bytes each)",
						       draw_count, index_size);
					} else {
						psilog_err("Failed reading glTF indices");
					}

					auto get_draw_mode = [](GLint primitive_mode) {
						GLint draw_mode;

						switch(primitive_mode) {
						case TINYGLTF_MODE_TRIANGLES:
							draw_mode = GL_TRIANGLES;
							break;
						case TINYGLTF_MODE_TRIANGLE_STRIP:
							draw_mode = GL_TRIANGLE_STRIP;
							break;
						case TINYGLTF_MODE_TRIANGLE_FAN:
							draw_mode = GL_TRIANGLE_FAN;
							break;
						case TINYGLTF_MODE_POINTS:
							draw_mode = GL_POINTS;
							break;
						case TINYGLTF_MODE_LINE:
							draw_mode = GL_LINES;
							break;
						case TINYGLTF_MODE_LINE_LOOP:
							draw_mode = GL_LINE_LOOP;
							break;
						default:
							draw_mode = GL_TRIANGLES;
						}

						return draw_mode;
					};
					draw_mode = get_draw_mode(primitive.mode);

					//plog_s("indexes draw_count=%d draw_mode=%d index_type = %d", draw_count, draw_mode, index_type);
					for (const auto &attribute : primitive.attributes) {
						std::string attr_name = attribute.first;
						std::string attr_accessor = attribute.second;

						auto accessor_it = scene.accessors.find(attr_accessor);
      					const tinygltf::Accessor &accessor = accessor_it->second;

						auto get_attrib_location = [](std::string attr_name) {
							GLuint loc;
							if (attr_name.compare("POSITION") == 0) {
								loc = PSIGLShader::AttribLocation::POSITION;
							} else if (attr_name.compare("NORMAL") == 0) {
								loc = PSIGLShader::AttribLocation::NORMAL;
							} else if (attr_name.compare("TEXCOORD_0") == 0) {
								loc = PSIGLShader::AttribLocation::TEXCOORD;
							} else {
								loc = PSIGLShader::AttribLocation::INVALID;
							}

							return loc;
						};
						GLuint loc = get_attrib_location(attr_name);

						auto get_attrib_size = [](GLint type) {
							GLint size;
							switch (type) {
							case TINYGLTF_TYPE_SCALAR:
								size = 1;
								break;
							case TINYGLTF_TYPE_VEC2:
								size = 2;
								break;
							case TINYGLTF_TYPE_VEC3:
								size = 3;
								break;
							case TINYGLTF_TYPE_VEC4:
								size = 4;
								break;
							default:
								size = 1;
								break;
							}

							return size;
						};
						GLint size = get_attrib_size(accessor.type);

						if (loc == PSIGLShader::AttribLocation::INVALID) {
							psilog(PSILog::OPENGL, "Skipping unmapped glTF attribute '%s'",
							       attr_name.c_str());
							continue;
						}

						// The shaders read every attribute as float. glTF may
						// store normalized integers instead, which would need
						// converting rather than copying.
						if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
							psilog_err("glTF attribute '%s' is component type %d, not float; skipping",
							           attr_name.c_str(), accessor.componentType);
							continue;
						}

						size_t element_size = size * gltf_component_size(accessor.componentType);
						std::vector<uint8_t> packed;
						if (!gltf_pack_accessor(scene, accessor, element_size, &packed)) {
							psilog_err("Failed reading glTF attribute '%s'", attr_name.c_str());
							continue;
						}

						// One tightly packed buffer per attribute, bound at the
						// attribute's own index.
						mesh_obj->bind_buffer(GL_ARRAY_BUFFER, loc);
						mesh_obj->buffer_data(GL_ARRAY_BUFFER, packed.size(),
						                      packed.data(), GL_STATIC_DRAW);
						mesh_obj->enable_vertex_attrib(loc, size, 0, nullptr, accessor.componentType);

						psilog(PSILog::OPENGL,
							"Packed glTF attribute %s -> location %d (%d comps, %d elements, src stride %d)",
							attr_name.c_str(), loc, size, (int)accessor.count, accessor.byteStride);
					}
				}
			}

			// Set draw mode and count for indexed drawing
			mesh_obj->set_draw_mode(draw_mode);
			mesh_obj->set_draw_count(draw_count);
			mesh_obj->set_index_type(index_type);

			psilog(PSILog::OPENGL, "Created mesh with draw_count = %d", draw_count);	
		}
	}

	return mesh_obj;
}

static std::string get_path_ext(const std::string &path) {
	if (path.find_last_of(".") != std::string::npos) {
		return path.substr(path.find_last_of(".") + 1);
	}

	return "";
}

GLMeshSharedPtr PSIGLTFLoader::load_gl_mesh(const ShaderSharedPtr &shader, std::string scene_path) {
	tinygltf::Scene scene;
	tinygltf::TinyGLTFLoader loader;

	std::string err;
	std::string ext = get_path_ext(scene_path);
	bool ret = false;
	if (ext.compare("glb") == 0) {
		// assume binary glTF.
		ret = loader.LoadBinaryFromFile(&scene, &err, scene_path.c_str());
	} else {
		// assume ascii glTF.
		ret = loader.LoadASCIIFromFile(&scene, &err, scene_path.c_str());
	}

	if (!err.empty()) {
		printf("Err: %s\n", err.c_str());
		return nullptr;
	}

	if (!ret) {
		printf("Failed to parse glTF\n");
		return nullptr;
	}

	/*
	printf("Loaded glTF scene succesfully!\n");
	for (auto &&node : scene.nodes) {
		std::cout << "node.name : " << node.second.name << std::endl;
	}
	*/

	assert(shader != nullptr);
	GLMeshSharedPtr mesh = create_gl_mesh(shader, scene);

	return mesh;
}

