// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Base class for any object being drawn with PSIEngine.
//
// Any class that is being drawn, uses this as the super class
// And must implement draw method.

#pragma once

#include "PSIGlobals.h"
#include "PSIGLShader.h"
#include "PSIGLMesh.h"
#include "PSIGLMaterial.h"
#include "PSIGLTransform.h"
#include "PSIRenderContext.h"
#include "PSIGeometryData.h"
#include "PSIAABB.h"

class PSIRenderObj;
typedef shared_ptr<PSIRenderObj> RenderObjSharedPtr;

class PSIRenderObj {
	public:
		// Physics body variables for a render obj.
		struct physics_body {
			// Directional velocity.
			glm::vec3 velocity;
			// Directional force applied.
			glm::vec3 force;
			// Only story the inverse of the mass.
			GLfloat mass_inv;
		};

		// Represents one asset in our GL rendering space.
		struct render_asset {
			// Mesh for the asset.
			GLMeshSharedPtr mesh;
			// Material.
			GLMaterialSharedPtr material;
			// Transformation.
			PSIGLTransform transform;
			// Physics transformation.
			PSIGLTransform p_transform;
			// Axis-aligned bounding box.
			PSIAABB aabb;
		};

		// All transformation matrices.
		struct transform_matrices {
			glm::mat4 model;
			glm::mat4 model_view;
			glm::mat4 projection;
			glm::mat4 model_view_projection;
		};

		// Optional "modules" to pass to the shader.
		// This is just a system for defining optional shader uniforms for now.
		enum ModulesType {
			MODULES_NONE = 0,
			// Passes elapsed time to the shader.
			MODULES_ELAPSED_TIME = 1
		};

		// Static creation method.
		static RenderObjSharedPtr create() {
			return make_shared<PSIRenderObj>();
		}

		PSIRenderObj();
		virtual ~PSIRenderObj() = default;

		// Copy constructor.
		PSIRenderObj(const PSIRenderObj &rhs);

		// Virtual methods for render obj.
		// Classes extending renderobj can implement these methods to have custom drawing and logic.
		// Logic is empty by default, implemented by the class extending.
		virtual void logic(const RenderContextSharedPtr &ctx) {
			return;
		}

		// Default implementation for drawing render obj. 
		virtual void draw(const RenderContextSharedPtr &ctx);

		// Empty default initialization method.
		virtual GLboolean init() {
			return true;
		}

		// Print class id.
		// Used to identify what class has extended render object.
		virtual void print_id() {
			std::cout << "PSIRenderObj" << std::endl;
		}

		// Init all geometry data buffers for this render obj.
		void init_buffers(const GLMeshSharedPtr &mesh, const GeometryDataSharedPtr &gpu_data);
		// Set all shader uniform values.
		void set_shader_uniforms();
		// Create GLMesh from gpu data.
		GLMeshSharedPtr create_gl_mesh(const GeometryDataSharedPtr &gpu_data);

		// Calculate transformation matrices.
		void calc_model_view_projection(const RenderContextSharedPtr &ctx,
		                                PSIGLTransform &transform);

		// Common methods shared between instances of PSIRenderObj.
		void draw_mesh() {
			// The staged uniform block is flushed inside the mesh draw, so every
			// draw path gets it (PSITextRenderer::draw() calls draw_indexed()
			// directly rather than coming through here).
			_render_asset.mesh->draw_indexed();
		}

		ShaderSharedPtr get_shader() {
			assert(_render_asset.material != nullptr);
			return _render_asset.material->get_shader();
		}

		// Draw-path accessor; see PSIGLMaterial::shader_ref(). The object owns
		// the material for the whole call, so the reference cannot dangle.
		const ShaderSharedPtr &get_shader_ref() const {
			assert(_render_asset.material != nullptr);
			return _render_asset.material->shader_ref();
		}

		void set_draw_mode(GLuint draw_mode) {
			assert(_render_asset.mesh != nullptr);
			_render_asset.mesh->set_draw_mode(draw_mode);
		}

		// Instancing, forwarded to the mesh.
		//
		// Scripts hold render objects, not the PSIGLMesh underneath, so these
		// save every caller a get_gl_mesh() hop. The mesh owns the data; see
		// PSIGLMesh.h for what an instance carries.
		//
		// The object's material must use a shader built with set_instanced(true),
		// otherwise the vertex shader will not read the instance buffer.
		void set_instance_count(GLuint count) {
			assert(_render_asset.mesh != nullptr);
			_render_asset.mesh->set_instance_count(count);
		}
		GLuint get_instance_count() {
			assert(_render_asset.mesh != nullptr);
			return _render_asset.mesh->get_instance_count();
		}
		void set_instance(GLuint index, PSIGLTransform &transform,
		                  const glm::vec4 &color, const glm::vec4 &custom) {
			assert(_render_asset.mesh != nullptr);
			_render_asset.mesh->set_instance(index, transform, color, custom);
		}
		void set_instance_transform(GLuint index, PSIGLTransform &transform) {
			assert(_render_asset.mesh != nullptr);
			_render_asset.mesh->set_instance_transform(index, transform);
		}
		void set_instance_color(GLuint index, const glm::vec4 &color) {
			assert(_render_asset.mesh != nullptr);
			_render_asset.mesh->set_instance_color(index, color);
		}
		void set_instance_custom(GLuint index, const glm::vec4 &custom) {
			assert(_render_asset.mesh != nullptr);
			_render_asset.mesh->set_instance_custom(index, custom);
		}
		void upload_instances() {
			assert(_render_asset.mesh != nullptr);
			_render_asset.mesh->upload_instances();
		}

		void set_transform(PSIGLTransform transform) {
			_render_asset.transform = transform;
		}
		PSIGLTransform& get_transform() {
			return _render_asset.transform;
		}
		void store_current_transform() {
			_render_asset.p_transform = _render_asset.transform;
		}

		glm::mat4 get_model_view_projection_matrix() {
			return _mvp.model_view_projection;
		}
		glm::mat4 get_model_view_matrix() {
			return _mvp.model_view;
		}
		glm::mat4 get_model_matrix() {
			return _mvp.model;
		}
		glm::mat4 get_projection_matrix() {
			return _mvp.projection;
		}
		// Inverse transpose of the model matrix's rotation/scale part.
		//
		// Cached against the matrix it was derived from, for the same reason
		// PSIGLTransform caches the model matrix: sixteen float compares instead
		// of a 3x3 inverse and transpose, per object per frame, and most objects
		// hold still.
		glm::mat3 get_normal_matrix() {
			if (!_normal_matrix_valid || _normal_matrix_src != _mvp.model) {
				_normal_matrix = glm::inverseTranspose(glm::mat3(_mvp.model));
				_normal_matrix_src = _mvp.model;
				_normal_matrix_valid = true;
			}
			return _normal_matrix;
		}

		void set_modules(GLint modules) {
			_modules = modules;
		}

		void set_geometry_data(const GeometryDataSharedPtr &geometry_data) {
			_geometry_data = geometry_data;
			// Bounds come from the geometry, and this is the one call every
			// path that attaches geometry makes -- including the shared-mesh
			// path in psi/obj.lua, which never calls create_gl_mesh().
			update_aabb_from_geometry();
		}
		GeometryDataSharedPtr get_geometry_data() {
			return _geometry_data;
		}

		// These are our children render objs, and are rendered and handled as a group when rendered.
		// TODO: Transformations, scaling and rotation should probably apply too.
		void add_child(RenderObjSharedPtr child) {
			_children.push_back(child);
		}
		RenderObjSharedPtr get_child(GLuint index) {
			return _children.at(index);
		}
		// By value: this is what Lua binds (LuaAPI.cpp).
		std::vector<RenderObjSharedPtr> get_children() {
			return _children;
		}
		// Draw-path accessor. The by-value form copies the vector and then the
		// range-for copied every shared_ptr in it again, on every object of
		// every frame -- for a list that is empty on almost all of them.
		const std::vector<RenderObjSharedPtr> &get_children_ref() const {
			return _children;
		}
		GLboolean has_children() {
			return _children.size() > 0;
		}
		GLsizei get_child_count() {
			return _children.size();
		}

		PSIAABB& get_aabb() {
			return _render_asset.aabb;
		}

		// Which of the shader's two pipelines this object needs; see
		// PSIGLMaterial::set_blending().
		bool wants_blending() const {
			assert(_render_asset.material != nullptr);
			return _render_asset.material->wants_blending();
		}

		// Fill the bounding box from this object's geometry. Load time only.
		void update_aabb_from_geometry();

		// May the renderer skip this object when its bounds fall outside the
		// camera frustum?
		//
		// Off for anything whose CPU-side bounds are not where the GPU actually
		// draws it -- a vertex shader that displaces geometry, for one. The
		// renderer already excludes the structural cases by itself (see
		// PSIGLRenderer::is_inside_frustum), so this is for what only the script
		// knows.
		void set_cullable(GLboolean cullable) {
			_cullable = cullable;
		}
		GLboolean is_cullable() {
			return _cullable;
		}

		void set_sort_index(GLfloat sort_index) {
			_sort_index = sort_index;
			_sort_index_set = true;
		}
		GLfloat get_sort_index() {
			return (_sort_index_set == true) ? _sort_index : _render_asset.transform.get_translation().z;
		}
		// Did a script place this object manually? See PSIRenderScene::sort().
		bool has_sort_index() const {
			return _sort_index_set;
		}

		void set_gl_mesh(GLMeshSharedPtr &mesh) {
			_render_asset.mesh = mesh;
		}
		GLMeshSharedPtr get_gl_mesh() {
			return _render_asset.mesh;
		}
		const GLMeshSharedPtr &get_gl_mesh_ref() const {
			return _render_asset.mesh;
		}

		void set_render_asset(render_asset &render_asset) {
			_render_asset = render_asset;
		}
		// By reference, not by value.
		//
		// render_asset is ~128 bytes holding two shared_ptrs, and the draw path
		// asked for a copy of it per object per frame -- four atomic refcount
		// operations plus the copy, to read two fields. Not const, because
		// calc_model_view_projection() writes through the transform's matrix
		// cache; see PSIGLTransform.
		render_asset &get_render_asset() {
			return _render_asset;
		}

		void set_physics_body(physics_body physics_body) {
			_physics_body = physics_body;
		}
		physics_body get_physics_body() {
			return _physics_body;
		}

		void set_interpolate_transform(GLboolean interpolate_transform) {
			_interpolate_transform = interpolate_transform;
		}
		GLboolean get_interpolate_transform() {
			return _interpolate_transform;
		}

		void set_mass(GLfloat mass) {
			// We actually just store the inverse of mass.
			_physics_body.mass_inv = 1.0f/mass;
		}
		GLfloat get_mass() {
			return (1.0f / _physics_body.mass_inv);
		}

		void set_velocity(glm::vec3 velocity) {
			_physics_body.velocity = velocity;
		}
		glm::vec3 get_velocity() {
			return _physics_body.velocity;
		}

		void set_force(glm::vec3 force) {
			_physics_body.force = force;
		}
		glm::vec3 get_force() {
			return _physics_body.force;
		}
		void add_force(glm::vec3 force) {
			_physics_body.force += force;
		}

		void set_material(GLMaterialSharedPtr material) {
			_render_asset.material = material;
		}
		GLMaterialSharedPtr get_material() {
			return _render_asset.material;
		}

		GLboolean is_depth_tested() {
			return _depth_tested;
		}
		void set_depth_tested(GLboolean depth_tested) {
			_depth_tested = depth_tested;
		}

		// Whether this object writes depth. Independent of the test above: a
		// blended object still wants to be hidden by opaque geometry in front of
		// it, but must not stamp depth that rejects the fragments behind it
		// before they can blend.
		GLboolean is_depth_written() {
			return _depth_written;
		}
		void set_depth_written(GLboolean depth_written) {
			_depth_written = depth_written;
		}

		void set_scene_index(GLuint scene_index) {
			_scene_index = scene_index;
		}
		GLuint get_scene_index() {
			return _scene_index;
		}

		void set_translated_by_camera(GLboolean camera_translated) {
			_camera_translated = camera_translated;
		}
		GLboolean is_translated_by_camera() {
			return _camera_translated;
		}

		void set_visible(GLboolean visible) {
			_visible = visible;
		}
		GLboolean is_visible() {
			return _visible;
		}

	private:
		// Model view projection.
		struct transform_matrices _mvp;

	public:
		// Does this object's mesh draw its geometry more than once per call?
		//
		// The velocity pass needs it before the object draws, to pick between
		// the plain and instanced stand-in shaders.
		bool is_instanced() const {
			const auto &mesh = get_gl_mesh_ref();
			return mesh != nullptr && mesh->is_instanced();
		}

	protected:
		// Compute this frame's matrices, keeping last frame's for the velocity
		// pass.
		//
		// Subclasses that override draw() -- PSITextRenderer, Poly -- must call
		// this rather than calc_model_view_projection() directly, or their
		// objects report motion from whatever _prev_mvp happened to hold.
		void calc_mvp_with_history(const RenderContextSharedPtr &ctx,
		                           PSIGLTransform &transform);

		const glm::mat4 &get_prev_model_view_projection_matrix() const {
			return _prev_mvp.model_view_projection;
		}

	private:
		// Where this object was last frame, for velocity.metal.
		//
		// Rolled over only while the velocity pass is encoding, which is once a
		// frame and always with the unjittered projection. Rolling it over in
		// draw() generally would be wrong twice: draw() runs more than once per
		// frame in a multi-pass script, and the scene pass's matrices carry the
		// TAA jitter, which is a property of the frame rather than of the
		// object and would show up as every static pixel reporting motion.
		struct transform_matrices _prev_mvp;
		uint64_t _prev_mvp_frame = 0;
		bool _has_prev_mvp = false;

		// Normal matrix cache; see get_normal_matrix().
		glm::mat3 _normal_matrix = glm::mat3(1.0f);
		glm::mat4 _normal_matrix_src = glm::mat4(1.0f);
		bool _normal_matrix_valid = false;

		// The render asset for this render obj.
		PSIRenderObj::render_asset _render_asset;
		// Geometry data for this render obj.
		GeometryDataSharedPtr _geometry_data;
		// Physics body for this render obj.
		PSIRenderObj::physics_body _physics_body;

		// Child render objs for this render obj.
		std::vector<RenderObjSharedPtr> _children;

		// Should this object be tested for depth ?
		GLboolean _depth_tested = true;
		GLboolean _depth_written = true;
		// Should this object be translated with the camera ?
		GLboolean _camera_translated = true;
		// Is our object visible, should it be drawn ?
		GLboolean _visible = true;
		// May frustum culling skip this object ? See set_cullable().
		GLboolean _cullable = true;
		// are we interpolating movement with physics state ?
		GLboolean _interpolate_transform = false;

		// This is our index in the scene.
		// Note that this assumes we only have one scene.
		// For multiple ones, a different approach would have to be devised.
		// -1 = invalid.
		GLuint _scene_index = -1;

		// This overrides the normal scene sorting based on the translation z value.
		// If this is set, then we use this as the scene sorting depth value.
		GLfloat _sort_index = 0.0f;
		bool _sort_index_set = false;

		// Currently set optional shader uniform modules.
		GLint _modules = ModulesType::MODULES_NONE;
};
