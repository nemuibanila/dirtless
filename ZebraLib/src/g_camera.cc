#include "g_camera.h"
#include "g_vec.h"
#include "z_debug.h"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_transform.hpp>

namespace zebra {
	void PerspectiveCamera::apply_movement(glm::vec3 dv) {
		_acc += dv;
	}
	void PerspectiveCamera::apply_rotation(float _phi, float _theta) {
		//this->phi += _phi;


		//this->phi = glm::abs(phi) > phi_limit ? glm::sign(phi) * phi_limit : phi;
		//this->theta += _theta;

		_rx = _rx * glm::angleAxis(_phi, vec::right);
		auto angle = glm::angle(_rx);
		if (glm::abs(angle) > glm::half_pi<float>()) {
			_rx = _rx * glm::angleAxis(-glm::sign(_phi) * (glm::abs(angle) - glm::half_pi<float>()), vec::right);
		}
		_ry = _ry * glm::angleAxis(_theta, vec::up);
	}
	void PerspectiveCamera::tick() {
		// smoothstep
		auto npos = glm::mix(_pos, _pos + _acc, (1.f/smoothing));
		_acc = _acc - (npos - _pos); 
		_pos = npos;

		// uprighting camera rotation

		const float correction_eps = 0.0001f;
		auto current_up = rotation() * vec::up;     
		   
		DBG("up: " << current_up.x << " " << current_up.y << " " << current_up.z);
	}

	glm::mat4 PerspectiveCamera::view() {
		return glm::toMat4(rotation()) * glm::translate(glm::mat4(1.f), _pos);
	}

	glm::quat PerspectiveCamera::rotation() {
		return _rx * _ry * base_rotation;
	}

	glm::mat4 PerspectiveCamera::projection() {
		auto projection = glm::perspective(povy, aspect, z_near, z_far);
		projection[1][1] *= -1; // for vulkan coordinates
		return projection;
	}

	glm::vec3 PerspectiveCamera::forward() {
		// CHECK THIS
		auto forward = glm::normalize(_ry) * vec::forward;
		forward.z = -forward.z;
		return forward;
	}

	glm::vec3 PerspectiveCamera::right() {
		auto right = glm::normalize(glm::cross(forward(), vec::up));
		return right;
	}

	glm::vec3 PerspectiveCamera::up() {
		return vec::up;
	}
}


