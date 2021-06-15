// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// A map class for holding and accessing shader uniform types.

#pragma once

#include "PSIGlobals.h"
#include <iostream>
#include <functional>
#include <unordered_map>

template <typename Type>
class PSITypeMap {
	private:
	public:

	PSITypeMap() = default;
	~PSITypeMap() = default;

	// The value store can store different types of data.
	// We can store a direct value, or a pointer to a value, or a function.
	enum class StoreType {
		TYPE_VALUE = 0,
		TYPE_POINTER = 1,
		TYPE_FUNCTION = 2
	};

	// Struct for storing a value and accessing it.
	struct value_store {
		// Used to determine value type and how to access it.
		StoreType type;

		// Depending on type, we store either the value.
		Type value;
		// Pointer to the value.
		Type *value_ptr;
		// Or a function reference.
		std::function<Type()> function;

		// Get value depending on type of value.
		auto get_value() {
			if (type == StoreType::TYPE_FUNCTION) {
				return function();
			}
			else if (type == StoreType::TYPE_POINTER) {
				return (*value_ptr);
			} else {
				return value;
			}
		}
	};

	// Map of values accessed by string.
	std::unordered_map<std::string, struct value_store> map;

	// Add direct value.
	void add(std::string name, Type val) {
		struct value_store store;
		store.value = val;
		store.type = StoreType::TYPE_VALUE;

		map.insert(std::make_pair(name, store));
	};

	// Add pointer to a value.
	void add(std::string name, Type *val) {
		struct value_store store;
		store.value_ptr = val;
		store.type = StoreType::TYPE_POINTER;

		map.insert(std::make_pair(name, store));
	};

	// Add function reference.
	void add(std::string name, std::function<Type()> func) {
		struct value_store store;
		store.function = func;
		store.type = StoreType::TYPE_FUNCTION;

		map.insert(std::make_pair(name, store));
	}

	// Print all keys and values in our map.
	void print_map() {
		std::cout << "map = " << std::endl;
		for (auto pair : map) {
			std::cout << "key = " << pair.first << std::endl;
		}
	}
};

class PSIUniformMap {
	private:
	public:
		PSIUniformMap() = default;
		~PSIUniformMap() = default;

		// Maps for each type of uniform values we can store.
		PSITypeMap<GLfloat> GLfloat_map;
		PSITypeMap<GLint> GLint_map;
		PSITypeMap<GLboolean> GLboolean_map;

		PSITypeMap<glm::vec3> vec3_map;
		PSITypeMap<glm::vec4> vec4_map;

		PSITypeMap<glm::mat3> mat3_map;
		PSITypeMap<glm::mat4> mat4_map;

		// Functions for adding each type of value in our maps.

		// Float.
		void add_float(std::string name, GLfloat val) {
			GLfloat_map.add(name, val);
		};
		void add_float(std::string name, GLfloat *val) {
			GLfloat_map.add(name, val);
		};
		void add_float(std::string name, std::function<GLfloat()> func) {
			GLfloat_map.add(name, func);
		}

		// glm::vec3.
		void add_vec3(std::string name, glm::vec3 val) {
			vec3_map.add(name, val);
		};
		void add_vec3(std::string name, glm::vec3 *val) {
			vec3_map.add(name, val);
		};
		void add_vec3(std::string name, std::function<glm::vec3()> func) {
			vec3_map.add(name, func);
		}

		// glm::vec4
		void add_vec4(std::string name, glm::vec4 val) {
			vec4_map.add(name, val);
		};
		void add_vec4(std::string name, glm::vec4 *val) {
			vec4_map.add(name, val);
		};
		void add_vec4(std::string name, std::function<glm::vec4()> func) {
			vec4_map.add(name, func);
		}

		// glm::mat3
		void add_mat3(std::string name, glm::mat3 val) {
			mat3_map.add(name, val);
		};
		void add_mat3(std::string name, glm::mat3 *val) {
			mat3_map.add(name, val);
		};
		void add_mat3(std::string name, std::function<glm::mat3()> func) {
			mat3_map.add(name, func);
		}

		// glm::mat4
		void add_mat4(std::string name, glm::mat4 val) {
			mat4_map.add(name, val);
		};
		void add_mat4(std::string name, glm::mat4 *val) {
			mat4_map.add(name, val);
		};
		void add_mat4(std::string name, std::function<glm::mat4()> func) {
			mat4_map.add(name, func);
		}

		// GLint
		void add_int(std::string name, GLint val) {
			GLint_map.add(name, val);
		};
		void add_int(std::string name, GLint *val) {
			GLint_map.add(name, val);
		};
		void add_int(std::string name, std::function<GLint()> func) {
			GLint_map.add(name, func);
		}

		// GLboolean
		void add_bool(std::string name, GLboolean val) {
			GLboolean_map.add(name, val);
		};
		void add_bool(std::string name, GLboolean *val) {
			GLboolean_map.add(name, val);
		};
		void add_bool(std::string name, std::function<GLboolean()> func) {
			GLboolean_map.add(name, func);
		}

		// Print all values in the map.
		void print_maps();

		// Clear all maps.
		void clear();
};
