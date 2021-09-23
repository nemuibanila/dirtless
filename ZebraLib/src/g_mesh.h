#pragma once
#include <glm/glm.hpp>
#include "g_types.h"
#include "g_buffer.h"
#include <filesystem>

namespace zebra {
	struct Material {
		VkDescriptorSet texture_set{ VK_NULL_HANDLE };
		VkPipeline pipeline;
		VkPipelineLayout pipeline_layout;
	};

	struct MeshPushConstants {
		glm::vec4 data;
		glm::mat4 render_matrix;
	};

	struct Mesh {
		std::vector<P3N3C3U2> _vertices;
		AllocBuffer _vertex_buffer;

		bool load_from_obj(const char* file);
	};

	struct GPUObjectData {
		glm::mat4 model_matrix;
		glm::vec4 color;
	};
}