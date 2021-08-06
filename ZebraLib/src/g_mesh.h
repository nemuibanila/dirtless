#pragma once
#include <glm/glm.hpp>
#include "g_types.h"
#include <filesystem>

namespace zebra {
	struct Material {
		VkPipeline pipeline;
		VkPipelineLayout pipeline_layout;
	};

	struct MeshPushConstants {
		glm::vec4 data;
		glm::mat4 render_matrix;
	};

	struct Mesh {
		std::vector<P3N3C3> _vertices;
		AllocBuffer _vertex_buffer;

		bool load_from_obj(const char* file);
	};

	struct RenderObject {
		Mesh* mesh;
		Material* material;
		glm::mat4 transform;
	};
}