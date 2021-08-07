#include "g_camera.h"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_transform.hpp>

namespace zebra {
	void PerspectiveCamera::apply_movement(glm::vec3 dv) {
		_acc += dv;
	}
	void PerspectiveCamera::apply_rotation(float _phi, float _theta) {
		this->phi += _phi;
		this->phi = glm::abs(phi) > phi_limit ? glm::sign(phi) * phi_limit : phi;
		this->theta += _theta;
	}
	void PerspectiveCamera::tick() {
		// smoothstep
		auto npos = glm::mix(_pos, _pos + _acc, (1.f/smoothing));
		_acc = _acc - (npos - _pos); 
		_pos = npos;
	}

	glm::mat4 PerspectiveCamera::view() {
		return glm::toMat4(rotation()) * glm::translate(glm::mat4(1.f), _pos);
	}

	glm::quat PerspectiveCamera::rotation() {
		// TODO FIXME
		return glm::quat(glm::vec3(phi, theta, 0.f));
	}

	glm::mat4 PerspectiveCamera::projection() {
		auto projection = glm::perspective(povy, aspect, z_near, z_far);
		projection[1][1] *= -1; // for vulkan coordinates
		return projection;
	}

	glm::vec3 PerspectiveCamera::forward() {
		// CHECK THIS
		auto inverted = glm::inverse(view());
		auto forward = normalize(glm::vec3(inverted[2]));
		return forward;
	}
}


