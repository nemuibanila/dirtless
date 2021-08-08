#pragma once
#include <glm/glm.hpp>
#include <glm/ext/quaternion_float.hpp>

namespace zebra {
	struct PerspectiveCamera {
		glm::mat4 view();
		glm::mat4 projection();
		glm::vec3 forward();
		glm::vec3 right();
		glm::vec3 up();
		
		glm::vec3 _pos;
		glm::vec3 _acc = glm::vec3(0.f);
		glm::quat rotation();
		glm::quat _rx = glm::quat(1.f, 0.f, 0.f, 0.f);
		glm::quat _ry = glm::quat(1.f, 0.f, 0.f, 0.f);
		glm::quat base_rotation = glm::quat(glm::vec3(0.f, glm::pi<float>(), 0.f));
		float movement_smoothing = 4.f;
		float camera_smoothing = 1.2f;
		float _rx_acc = 0.f;
		float _ry_acc = 0.f;

		float aspect;
		float povy;

		float z_near;
		float z_far;

		//float phi;
		//float phi_limit = 0.95f * glm::half_pi<float>();
		//float theta;

		void apply_movement(glm::vec3 dv);
		void apply_rotation(float phi, float theta);
		void tick();
	};
}