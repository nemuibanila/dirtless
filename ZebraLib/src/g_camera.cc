#include "g_camera.h"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_transform.hpp>

namespace zebra {
	void PerspectiveCamera::apply_movement(glm::vec3 dv) {
		_acc += dv;
	}
	void PerspectiveCamera::apply_rotation(glm::quat rotation) {
		_rotation = rotation * _rotation;
	}
	void PerspectiveCamera::tick() {
		// smoothstep
		auto npos = glm::mix(_pos, _pos + _acc, (1.f/smoothing));
		_acc = _acc - (npos - _pos);
		_pos = npos;
	}

	glm::mat4 PerspectiveCamera::view() {
		return glm::toMat4(_rotation) * glm::translate(glm::mat4(1.f), _pos);
	}

	glm::mat4 PerspectiveCamera::projection() {
		auto projection = glm::perspective(povy, aspect, z_near, z_far);
		//projection[1][1] *= -1; // for vulkan coordinates
		return projection;
	}

	glm::vec3 PerspectiveCamera::forward() {
		auto inverted = glm::inverse(view());
		auto forward = normalize(glm::vec3(inverted[2]));
		return forward;
	}
}


