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
#include "PSIUniformMap.h"
#include "PSIAABB.h"

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
			shared_ptr<PSIGLMesh> mesh;
			// Material.
			shared_ptr<PSIGLMaterial> material;
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

		PSIRenderObj();
		virtual ~PSIRenderObj() = default;

		// Copy constructor.
		PSIRenderObj(const PSIRenderObj &rhs);

		// Uniform mapper for storing different types of uniform values.
		// This is public, so we can add new uniforms easily.
		unique_ptr<PSIUniformMap> uniforms;

		// Virtual methods for render obj.
		// Classes extending renderobj can implement these methods to have custom drawing and logic.

		// Logic is empty by default, implemented by the class extending.
		virtual void logic(const shared_ptr<PSIRenderContext> &ctx) {
			return;
		}

		// Default implementation for drawing render obj. 
		virtual void draw(const shared_ptr<PSIRenderContext> &ctx);

		// Empty default initialization method.
		virtual GLboolean init() {
			return true;
		}

		// Print class id.
		// Used to identify what class has extended render object.
		virtual void print_id() {
			std::cout << "PSIRenderObj" << std::endl;
		}

		// Setup shader uniforms for lights.
		void setup_lights(const shared_ptr<PSIGLShader> &shader,
		                  const shared_ptr<PSIRenderContext> &ctx,
		                  render_asset &inst);

		// Init all geometry data buffers for this render obj.
		void init_buffers(const shared_ptr<PSIGLMesh> &mesh, const shared_ptr<PSIGeometryData> &gpu_data);
		// Set all shader uniform values.
		void set_shader_uniforms();
		// Create GLMesh from gpu data.
		shared_ptr<PSIGLMesh> create_gl_mesh(const shared_ptr<PSIGeometryData> &gpu_data);

		// Calculate transformation matrices.
		void calc_model_view_projection(const shared_ptr<PSIRenderContext> &ctx,
		                                PSIGLTransform &transform);

		// Common methods shared between instances of PSIRenderObj.
		void draw_mesh() {
			//psilog(PSILog::FREQ, "Drawing mesh");
			_render_asset.mesh->draw_indexed();
		}

		shared_ptr<PSIGLShader> get_shader() {
			return _render_asset.material->get_shader();
		}

		void set_draw_mode(GLuint draw_mode) {
			assert(_render_asset.mesh != nullptr);
			_render_asset.mesh->set_draw_mode(draw_mode);
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
		glm::mat3 get_normal_matrix() {
			return glm::inverseTranspose(glm::mat3(_mvp.model));
		}

		void set_modules(GLint modules) {
			_modules = modules;
		}

		void set_geometry_data(const shared_ptr<PSIGeometryData> &geometry_data) {
			_geometry_data = geometry_data;
		}
		shared_ptr<PSIGeometryData> get_geometry_data() {
			return _geometry_data;
		}

		// These are our children render objs, and are rendered and handled as a group when rendered.
		// TODO: Transformations, scaling and rotation should probably apply too.
		void add_child(shared_ptr<PSIRenderObj> child) {
			_children.push_back(child);
		}
		shared_ptr<PSIRenderObj> get_child(GLuint index) {
			return _children.at(index);
		}
		std::vector<shared_ptr<PSIRenderObj>> get_children() {
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

		void set_sort_index(GLfloat sort_index) {
			_sort_index = sort_index;
			_sort_index_set = true;
		}
		GLfloat get_sort_index() {
			return (_sort_index_set == true) ? _sort_index : _render_asset.transform.get_translation().z;
		}

		void set_gl_mesh(shared_ptr<PSIGLMesh> &mesh) {
			_render_asset.mesh = mesh;
		}
		shared_ptr<PSIGLMesh> get_gl_mesh() {
			return _render_asset.mesh;
		}

		void set_render_asset(render_asset &render_asset) {
			_render_asset = render_asset;
		}
		render_asset get_render_asset() {
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

		void set_material(shared_ptr<PSIGLMaterial> material) {
			_render_asset.material = material;
		}
		shared_ptr<PSIGLMaterial> get_material() {
			return _render_asset.material;
		}

		GLboolean is_depth_tested() {
			return _depth_tested;
		}
		void set_depth_tested(GLboolean depth_tested) {
			_depth_tested = depth_tested;
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

		// The render asset for this render obj.
		PSIRenderObj::render_asset _render_asset;
		// Geometry data for this render obj.
		shared_ptr<PSIGeometryData> _geometry_data;
		// Physics body for this render obj.
		PSIRenderObj::physics_body _physics_body;

		// Child render objs for this render obj.
		std::vector<shared_ptr<PSIRenderObj>> _children;

		// Should this object be tested for depth ?
		GLboolean _depth_tested = true;
		// Should this object be translated with the camera ?
		GLboolean _camera_translated = true;
		// Is our object visible, should it be drawn ?
		GLboolean _visible = true;
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
