#include "g_camera.h"
#include "g_vec.h"
#include "z_debug.h"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_transform.hpp>

namespace zebra {
	void FirstPersonPerspectiveCamera::apply_movement(glm::vec3 dv) {
		_acc += dv;
	}
	void FirstPersonPerspectiveCamera::apply_rotation(float _phi, float _theta) {
		//this->phi += _phi;


		//this->phi = glm::abs(phi) > phi_limit ? glm::sign(phi) * phi_limit : phi;
		//this->theta += _theta;

		auto defer_x = (1.f - (1.f / camera_smoothing)) * _phi;
		_rx_acc += defer_x;
		_phi -= defer_x;

		auto defer_y = (1.f - (1.f / camera_smoothing)) * _theta;
		_ry_acc += defer_y;
		_theta -= defer_y;

		_rx = _rx * glm::angleAxis(_phi, vec::right);
		auto angle = glm::angle(_rx);
		if (glm::abs(angle) > glm::half_pi<float>()) {
			_rx = _rx * glm::angleAxis(-glm::sign(_phi) * (glm::abs(angle) - glm::half_pi<float>()), vec::right);
		}
		_ry = _ry * glm::angleAxis(_theta, vec::up);
	}
	void FirstPersonPerspectiveCamera::tick() {
		// smoothstep
		auto npos = glm::mix(_pos, _pos + _acc, (1.f/movement_smoothing));
		_acc = _acc - (npos - _pos); 
		_pos = npos;

		// uprighting camera rotation -- not 
		//const float correction_eps = 0.0001f;
		//auto current_up = rotation() * vec::up;     
		//DBG("up: " << current_up.x << " " << current_up.y << " " << current_up.z);

		// camera smoothing 
		auto accx = _rx_acc;
		_rx_acc = 0.f;

		auto accy = _ry_acc;
		_ry_acc = 0.f;
		apply_rotation(accx, accy);
	}

	glm::mat4 FirstPersonPerspectiveCamera::view() {
		return glm::toMat4(rotation()) * glm::translate(glm::mat4(1.f), _pos);
	}

	glm::quat FirstPersonPerspectiveCamera::rotation() {
		return _rx * _ry * base_rotation;
	}

	glm::mat4 FirstPersonPerspectiveCamera::projection() {
		auto projection = glm::perspective(povy, aspect, z_near, z_far);
		projection[1][1] *= -1; // for vulkan coordinates
		return projection;
	}

	glm::vec3 FirstPersonPerspectiveCamera::forward() {
		// CHECK THIS
		auto forward = glm::normalize(_ry) * vec::forward;
		forward.z = -forward.z;
		return forward;
	}

	glm::vec3 FirstPersonPerspectiveCamera::right() {
		auto right = glm::normalize(glm::cross(forward(), vec::up));
		return right;
	}

	glm::vec3 FirstPersonPerspectiveCamera::up() {
		return vec::up;
	}
}


