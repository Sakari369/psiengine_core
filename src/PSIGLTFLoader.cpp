#include "PSIGLTFLoader.h"

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

shared_ptr<PSIGLMesh> PSIGLTFLoader::create_gl_mesh(const shared_ptr<PSIGLShader> &shader, const tinygltf::Scene &scene) {
	psilog(PSILog::OPENGL, "Creating mesh from glTF scene");

	// Create new mesh object.
	shared_ptr<PSIGLMesh> mesh_obj = make_shared<PSIGLMesh>();
	mesh_obj->gen_vao();
	mesh_obj->bind_vao();

	std::map<std::string, GLuint> buffer_ids;

	for (const auto &it : scene.bufferViews) {
		const tinygltf::BufferView &view = it.second;

		if (view.target == 0) {
			std::cout << "WARN: view.target is zero" << std::endl;
			continue;  // Unsupported view.
		}

		const tinygltf::Buffer &buffer = scene.buffers.at(view.buffer);
		assert((view.byteOffset + view.byteLength) <= buffer.data.size());

		// We have one buffer per buffer view.
		// Generate the buffer for that.
		GLuint buffer_id;
		glGenBuffers(1, &buffer_id);
		glBindBuffer(view.target, buffer_id);
		glBufferData(view.target, view.byteLength, &buffer.data.at(0) + view.byteOffset, GL_STATIC_DRAW);

		psilog(PSILog::OPENGL, "view.target = %d .buffer = %s .byteOffset = %d .byteLength = %d", 
					view.target, view.buffer.c_str(), view.byteOffset, view.byteLength);

		// Store the id for later reference.
		buffer_ids[it.first] = buffer_id;
	}

	/*
	for (auto buffer_id : buffer_ids) {
		plog_s("buffer_ids[%s] = %d", buffer_id.first.c_str(), buffer_id.second);
	}
	*/

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

						// At this point we should have all the data to setup the vertex attributes
						glBindBuffer(GL_ARRAY_BUFFER, buffer_ids[accessor.bufferView]);

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

						glVertexAttribPointer(loc, size, accessor.componentType, GL_FALSE,
								      accessor.byteStride, BUFFER_OFFSET(accessor.byteOffset));
						glEnableVertexAttribArray(loc);

						psilog(PSILog::OPENGL, 
							"Enabled vertex attrib %s for location = %d, size = %d stride = %d offset = %d type = %d", 
							accessor_it->first.c_str(), loc, size, 
							accessor.byteStride, accessor.byteOffset, accessor.componentType);
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

shared_ptr<PSIGLMesh> PSIGLTFLoader::load_gl_mesh(const shared_ptr<PSIGLShader> &shader, std::string scene_path) {
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
	shared_ptr<PSIGLMesh> mesh = create_gl_mesh(shader, scene);

	return mesh;
}

