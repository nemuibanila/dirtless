#pragma once
#include <glm/glm.hpp>
#include <iostream>

namespace zebra {
	namespace vec {
		constexpr glm::vec3 up{ 0.f, 1.f, 0.f };
		constexpr glm::vec3 forward{ 0.f, 0.f, 1.f };
		constexpr glm::vec3 right{ 1.f, 0.f, 0.f };
	}

	inline std::ostream& operator<<(std::ostream& os, glm::vec3 const& vec) {
		return os << "x: " << vec.x << "y: " << vec.y << "z: " << vec.z;
	}
}