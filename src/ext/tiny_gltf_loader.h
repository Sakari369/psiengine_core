//
// Tiny glTF loader.
//
//
// The MIT License (MIT)
//
// Copyright (c) 2015 - 2016 Syoyo Fujita and many contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// Version:
//  - v0.9.5 Support parsing `extras` parameter.
//  - v0.9.4 Support parsing `shader`, `program` and `tecnique` thanks to
//  @lukesanantonio
//  - v0.9.3 Support binary glTF
//  - v0.9.2 Support parsing `texture`
//  - v0.9.1 Support loading glTF asset from memory
//  - v0.9.0 Initial
//
// Tiny glTF loader is using following third party libraries:
//
//  - picojson: C++ JSON library.
//  - base64: base64 decode/encode library.
//  - stb_image: Image loading library.
//
#ifndef TINY_GLTF_LOADER_H_
#define TINY_GLTF_LOADER_H_

#include <cassert>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace tinygltf {

#define TINYGLTF_MODE_POINTS (0)
#define TINYGLTF_MODE_LINE (1)
#define TINYGLTF_MODE_LINE_LOOP (2)
#define TINYGLTF_MODE_TRIANGLES (4)
#define TINYGLTF_MODE_TRIANGLE_STRIP (5)
#define TINYGLTF_MODE_TRIANGLE_FAN (6)

#define TINYGLTF_COMPONENT_TYPE_BYTE (5120)
#define TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE (5121)
#define TINYGLTF_COMPONENT_TYPE_SHORT (5122)
#define TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT (5123)
#define TINYGLTF_COMPONENT_TYPE_INT (5124)
#define TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT (5125)
#define TINYGLTF_COMPONENT_TYPE_FLOAT (5126)
#define TINYGLTF_COMPONENT_TYPE_DOUBLE (5127)

#define TINYGLTF_TEXTURE_FILTER_NEAREST (9728)
#define TINYGLTF_TEXTURE_FILTER_LINEAR (9729)
#define TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST (9984)
#define TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST (9985)
#define TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR (9986)
#define TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR (9987)

#define TINYGLTF_TEXTURE_WRAP_RPEAT (10497)
#define TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE (33071)
#define TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT (33648)

// Redeclarations of the above for technique.parameters.
#define TINYGLTF_PARAMETER_TYPE_BYTE (5120)
#define TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE (5121)
#define TINYGLTF_PARAMETER_TYPE_SHORT (5122)
#define TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT (5123)
#define TINYGLTF_PARAMETER_TYPE_INT (5124)
#define TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT (5125)
#define TINYGLTF_PARAMETER_TYPE_FLOAT (5126)

#define TINYGLTF_PARAMETER_TYPE_FLOAT_VEC2 (35664)
#define TINYGLTF_PARAMETER_TYPE_FLOAT_VEC3 (35665)
#define TINYGLTF_PARAMETER_TYPE_FLOAT_VEC4 (35666)

#define TINYGLTF_PARAMETER_TYPE_INT_VEC2 (35667)
#define TINYGLTF_PARAMETER_TYPE_INT_VEC3 (35668)
#define TINYGLTF_PARAMETER_TYPE_INT_VEC4 (35669)

#define TINYGLTF_PARAMETER_TYPE_BOOL (35670)
#define TINYGLTF_PARAMETER_TYPE_BOOL_VEC2 (35671)
#define TINYGLTF_PARAMETER_TYPE_BOOL_VEC3 (35672)
#define TINYGLTF_PARAMETER_TYPE_BOOL_VEC4 (35673)

#define TINYGLTF_PARAMETER_TYPE_FLOAT_MAT2 (35674)
#define TINYGLTF_PARAMETER_TYPE_FLOAT_MAT3 (35675)
#define TINYGLTF_PARAMETER_TYPE_FLOAT_MAT4 (35676)

#define TINYGLTF_PARAMETER_TYPE_SAMPLER_2D (35678)

// End parameter types

#define TINYGLTF_TYPE_VEC2 (2)
#define TINYGLTF_TYPE_VEC3 (3)
#define TINYGLTF_TYPE_VEC4 (4)
#define TINYGLTF_TYPE_MAT2 (32 + 2)
#define TINYGLTF_TYPE_MAT3 (32 + 3)
#define TINYGLTF_TYPE_MAT4 (32 + 4)
#define TINYGLTF_TYPE_SCALAR (64 + 1)
#define TINYGLTF_TYPE_VECTOR (64 + 4)
#define TINYGLTF_TYPE_MATRIX (64 + 16)

#define TINYGLTF_IMAGE_FORMAT_JPEG (0)
#define TINYGLTF_IMAGE_FORMAT_PNG (1)
#define TINYGLTF_IMAGE_FORMAT_BMP (2)
#define TINYGLTF_IMAGE_FORMAT_GIF (3)

#define TINYGLTF_TEXTURE_FORMAT_ALPHA (6406)
#define TINYGLTF_TEXTURE_FORMAT_RGB (6407)
#define TINYGLTF_TEXTURE_FORMAT_RGBA (6408)
#define TINYGLTF_TEXTURE_FORMAT_LUMINANCE (6409)
#define TINYGLTF_TEXTURE_FORMAT_LUMINANCE_ALPHA (6410)

#define TINYGLTF_TEXTURE_TARGET_TEXTURE2D (3553)
#define TINYGLTF_TEXTURE_TYPE_UNSIGNED_BYTE (5121)

#define TINYGLTF_TARGET_ARRAY_BUFFER (34962)
#define TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER (34963)

#define TINYGLTF_SHADER_TYPE_VERTEX_SHADER (35633)
#define TINYGLTF_SHADER_TYPE_FRAGMENT_SHADER (35632)

typedef enum {
  NULL_TYPE = 0,
  NUMBER_TYPE = 1,
  INT_TYPE = 2,
  BOOL_TYPE = 3,
  STRING_TYPE = 4,
  ARRAY_TYPE = 5,
  BINARY_TYPE = 6,
  OBJECT_TYPE = 7
} Type;

// Simple class to represent JSON object
class Value {
 public:
  typedef std::vector<Value> Array;
  typedef std::map<std::string, Value> Object;

  Value() : type_(NULL_TYPE) {}

  explicit Value(bool b) : type_(BOOL_TYPE) { boolean_value_ = b; }
  explicit Value(int i) : type_(INT_TYPE) { int_value_ = i; }
  explicit Value(double n) : type_(NUMBER_TYPE) { number_value_ = n; }
  explicit Value(const std::string &s) : type_(STRING_TYPE) {
    string_value_ = s;
  }
  explicit Value(const unsigned char *p, size_t n) : type_(BINARY_TYPE) {
    binary_value_.resize(n);
    memcpy(binary_value_.data(), p, n);
  }
  explicit Value(const Array &a) : type_(ARRAY_TYPE) {
    array_value_ = Array(a);
  }
  explicit Value(const Object &o) : type_(OBJECT_TYPE) {
    object_value_ = Object(o);
  }

  char Type() const { return static_cast<const char>(type_); }

  bool IsBool() const { return (type_ == BOOL_TYPE); }

  bool IsInt() const { return (type_ == INT_TYPE); }

  bool IsNumber() const { return (type_ == NUMBER_TYPE); }

  bool IsString() const { return (type_ == STRING_TYPE); }

  bool IsBinary() const { return (type_ == BINARY_TYPE); }

  bool IsArray() const { return (type_ == ARRAY_TYPE); }

  bool IsObject() const { return (type_ == OBJECT_TYPE); }

  // Accessor
  template <typename T>
  const T &Get() const;
  template <typename T>
  T &Get();

  // Lookup value from an array
  const Value &Get(int idx) const {
    static Value &null_value = *(new Value());
    assert(IsArray());
    assert(idx >= 0);
    return (static_cast<size_t>(idx) < array_value_.size())
               ? array_value_[static_cast<size_t>(idx)]
               : null_value;
  }

  // Lookup value from a key-value pair
  const Value &Get(const std::string &key) const {
    static Value &null_value = *(new Value());
    assert(IsObject());
    Object::const_iterator it = object_value_.find(key);
    return (it != object_value_.end()) ? it->second : null_value;
  }

  size_t ArrayLen() const {
    if (!IsArray()) return 0;
    return array_value_.size();
  }

  // Valid only for object type.
  bool Has(const std::string &key) const {
    if (!IsObject()) return false;
    Object::const_iterator it = object_value_.find(key);
    return (it != object_value_.end()) ? true : false;
  }

  // List keys
  std::vector<std::string> Keys() const {
    std::vector<std::string> keys;
    if (!IsObject()) return keys;  // empty

    for (Object::const_iterator it = object_value_.begin();
         it != object_value_.end(); ++it) {
      keys.push_back(it->first);
    }

    return keys;
  }

 protected:
  int type_;

  int int_value_;
  double number_value_;
  std::string string_value_;
  std::vector<unsigned char> binary_value_;
  Array array_value_;
  Object object_value_;
  bool boolean_value_;
  char pad[3];

  int pad0;
};

#define TINYGLTF_VALUE_GET(ctype, var)            \
  template <>                                     \
  inline const ctype &Value::Get<ctype>() const { \
    return var;                                   \
  }                                               \
  template <>                                     \
  inline ctype &Value::Get<ctype>() {             \
    return var;                                   \
  }
TINYGLTF_VALUE_GET(bool, boolean_value_)
TINYGLTF_VALUE_GET(double, number_value_)
TINYGLTF_VALUE_GET(int, int_value_)
TINYGLTF_VALUE_GET(std::string, string_value_)
TINYGLTF_VALUE_GET(std::vector<unsigned char>, binary_value_)
TINYGLTF_VALUE_GET(Value::Array, array_value_)
TINYGLTF_VALUE_GET(Value::Object, object_value_)
#undef TINYGLTF_VALUE_GET

typedef struct {
  std::string string_value;
  std::vector<double> number_array;
} Parameter;

typedef std::map<std::string, Parameter> ParameterMap;

typedef struct {
  std::string sampler;
  std::string target_id;
  std::string target_path;
  Value extras;
} AnimationChannel;

typedef struct {
  std::string input;
  std::string interpolation;
  std::string output;
  Value extras;
} AnimationSampler;

typedef struct {
  std::string name;
  std::vector<AnimationChannel> channels;
  std::map<std::string, AnimationSampler> samplers;
  ParameterMap parameters;
  Value extras;
} Animation;

typedef struct {
  std::string name;
  int minFilter;
  int magFilter;
  int wrapS;
  int wrapT;
  int wrapR;  // TinyGLTF extension
  int pad0;
  Value extras;
} Sampler;

typedef struct {
  std::string name;
  int width;
  int height;
  int component;
  int pad0;
  std::vector<unsigned char> image;

  std::string bufferView;  // KHR_binary_glTF extenstion.
  std::string mimeType;    // KHR_binary_glTF extenstion.

  Value extras;
} Image;

typedef struct {
  int format;
  int internalFormat;
  std::string sampler;  // Required
  std::string source;   // Required
  int target;
  int type;
  std::string name;
  Value extras;
} Texture;

typedef struct {
  std::string name;
  std::string technique;
  ParameterMap values;

  Value extras;
} Material;

typedef struct {
  std::string name;
  std::string buffer;  // Required
  size_t byteOffset;   // Required
  size_t byteLength;   // default: 0
  int target;
  int pad0;
  Value extras;
} BufferView;

typedef struct {
  std::string bufferView;
  std::string name;
  size_t byteOffset;
  size_t byteStride;
  int componentType;  // One of TINYGLTF_COMPONENT_TYPE_***
  int pad0;
  size_t count;
  int type;  // One of TINYGLTF_TYPE_***
  int pad1;
  std::vector<double> minValues;  // Optional
  std::vector<double> maxValues;  // Optional
  Value extras;
} Accessor;

class Camera {
 public:
  Camera() {}
  ~Camera() {}

  std::string name;
  bool isOrthographic;  // false = perspective.

  // Some common properties.
  float aspectRatio;
  float yFov;
  float zFar;
  float zNear;
};

typedef struct {
  std::map<std::string, std::string> attributes;  // A dictionary object of
                                                  // strings, where each string
                                                  // is the ID of the accessor
                                                  // containing an attribute.
  std::string material;  // The ID of the material to apply to this primitive
                         // when rendering.
  std::string indices;   // The ID of the accessor that contains the indices.
  int mode;              // one of TINYGLTF_MODE_***
  int pad0;

  Value extras;  // "extra" property
} Primitive;

typedef struct {
  std::string name;
  std::vector<Primitive> primitives;
  Value extras;
} Mesh;

class Node {
 public:
  Node() {}
  ~Node() {}

  std::string camera;  // camera object referenced by this node.

  std::string name;
  std::vector<std::string> children;
  std::vector<double> rotation;     // length must be 0 or 4
  std::vector<double> scale;        // length must be 0 or 3
  std::vector<double> translation;  // length must be 0 or 3
  std::vector<double> matrix;       // length must be 0 or 16
  std::vector<std::string> meshes;

  Value extras;
};

typedef struct {
  std::string name;
  std::vector<unsigned char> data;
  Value extras;
} Buffer;

typedef struct {
  std::string name;
  int type;
  int pad0;
  std::vector<unsigned char> source;

  Value extras;
} Shader;

typedef struct {
  std::string name;
  std::string vertexShader;
  std::string fragmentShader;
  std::vector<std::string> attributes;

  Value extras;
} Program;

typedef struct {
  int count;
  int pad0;
  std::string node;
  std::string semantic;
  int type;
  int pad1;
  Parameter value;
} TechniqueParameter;

typedef struct {
  std::string name;
  std::string program;
  std::map<std::string, TechniqueParameter> parameters;
  std::map<std::string, std::string> attributes;
  std::map<std::string, std::string> uniforms;

  Value extras;
} Technique;

typedef struct {
  std::string generator;
  std::string version;
  std::string profile_api;
  std::string profile_version;
  bool premultipliedAlpha;
  char pad[7];
  Value extras;
} Asset;

class Scene {
 public:
  Scene() {}
  ~Scene() {}

  std::map<std::string, Accessor> accessors;
  std::map<std::string, Animation> animations;
  std::map<std::string, Buffer> buffers;
  std::map<std::string, BufferView> bufferViews;
  std::map<std::string, Material> materials;
  std::map<std::string, Mesh> meshes;
  std::map<std::string, Node> nodes;
  std::map<std::string, Texture> textures;
  std::map<std::string, Image> images;
  std::map<std::string, Shader> shaders;
  std::map<std::string, Program> programs;
  std::map<std::string, Technique> techniques;
  std::map<std::string, Sampler> samplers;
  std::map<std::string, std::vector<std::string> > scenes;  // list of nodes

  std::string defaultScene;

  Asset asset;

  Value extras;
};

enum SectionCheck {
  NO_REQUIRE = 0x00,
  REQUIRE_SCENE = 0x01,
  REQUIRE_SCENES = 0x02,
  REQUIRE_NODES = 0x04,
  REQUIRE_ACCESSORS = 0x08,
  REQUIRE_BUFFERS = 0x10,
  REQUIRE_BUFFER_VIEWS = 0x20,
  REQUIRE_ALL = 0x3f
};

class TinyGLTFLoader {
 public:
  TinyGLTFLoader() : bin_data_(NULL), bin_size_(0), is_binary_(false) {
    pad[0] = pad[1] = pad[2] = pad[3] = pad[4] = pad[5] = pad[6] = 0;
  }
  ~TinyGLTFLoader() {}

  /// Loads glTF ASCII asset from a file.
  /// Returns false and set error string to `err` if there's an error.
  bool LoadASCIIFromFile(Scene *scene, std::string *err,
                         const std::string &filename,
                         unsigned int check_sections = REQUIRE_ALL);

  /// Loads glTF ASCII asset from string(memory).
  /// `length` = strlen(str);
  /// Returns false and set error string to `err` if there's an error.
  bool LoadASCIIFromString(Scene *scene, std::string *err, const char *str,
                           const unsigned int length,
                           const std::string &base_dir,
                           unsigned int check_sections = REQUIRE_ALL);

  /// Loads glTF binary asset from a file.
  /// Returns false and set error string to `err` if there's an error.
  bool LoadBinaryFromFile(Scene *scene, std::string *err,
                          const std::string &filename,
                          unsigned int check_sections = REQUIRE_ALL);

  /// Loads glTF binary asset from memory.
  /// `length` = strlen(str);
  /// Returns false and set error string to `err` if there's an error.
  bool LoadBinaryFromMemory(Scene *scene, std::string *err,
                            const unsigned char *bytes,
                            const unsigned int length,
                            const std::string &base_dir = "",
                            unsigned int check_sections = REQUIRE_ALL);

 private:
  /// Loads glTF asset from string(memory).
  /// `length` = strlen(str);
  /// Returns false and set error string to `err` if there's an error.
  bool LoadFromString(Scene *scene, std::string *err, const char *str,
                      const unsigned int length, const std::string &base_dir,
                      unsigned int check_sections);

  const unsigned char *bin_data_;
  size_t bin_size_;
  bool is_binary_;
  char pad[7];
};

}  // namespace tinygltf

#endif  // TINY_GLTF_LOADER_H_
