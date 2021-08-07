#pragma once
#include <glm/glm.hpp>
#include <glm/ext/quaternion_float.hpp>

namespace zebra {
	struct PerspectiveCamera {
		glm::mat4 view();
		glm::mat4 projection();
		glm::vec3 forward();
		
		glm::vec3 _pos;
		glm::vec3 _acc = glm::vec3(0.f);
		glm::quat _rotation;
		float smoothing;

		float aspect;
		float povy;

		float z_near;
		float z_far;

		void apply_movement(glm::vec3 dv);
		void apply_rotation(glm::quat rotation);
		void tick();
	};
}