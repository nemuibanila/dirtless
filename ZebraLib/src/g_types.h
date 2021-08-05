#pragma once
#include <vk_mem_alloc.h>
#include <vector>
#include <glm/vec3.hpp>

struct AllocatedBuffer {
	VkBuffer buffer;
	VmaAllocation allocation;
};

struct P3N3C3 {
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec3 color;
};

struct Mesh {
	std::vector<P3N3C3> _vertices;
	AllocatedBuffer _vertex_buffer;
};