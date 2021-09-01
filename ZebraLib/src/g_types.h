#pragma once
#include <vk_mem_alloc.h>
#include <vector>
#include <glm/glm.hpp>

namespace zebra {

	struct AllocImage {
		VkImage image;
		VmaAllocation allocation;
	};

	struct Texture {
		AllocImage image;
		VkImageView view;
	};

	struct VertexInputDescription {
		std::vector<VkVertexInputBindingDescription> bindings;
		std::vector<VkVertexInputAttributeDescription> attributes;
		VkPipelineVertexInputStateCreateFlags flags = 0;
	};

	struct P3N3C3 {
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec3 color;

		static constexpr VertexInputDescription get_vertex_description() {
			VertexInputDescription description;

			//we will have just 1 vertex buffer binding, with a per-vertex rate
			VkVertexInputBindingDescription mainBinding = {};
			mainBinding.binding = 0;
			mainBinding.stride = sizeof(P3N3C3);
			mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			description.bindings.push_back(mainBinding);

			//Position will be stored at Location 0
			VkVertexInputAttributeDescription positionAttribute = {};
			positionAttribute.binding = 0;
			positionAttribute.location = 0;
			positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
			positionAttribute.offset = offsetof(P3N3C3, pos);
			
			//Normal will be stored at Location 1
			VkVertexInputAttributeDescription normalAttribute = {};
			normalAttribute.binding = 0;
			normalAttribute.location = 1;
			normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
			normalAttribute.offset = offsetof(P3N3C3, normal);

			//Color will be stored at Location 2
			VkVertexInputAttributeDescription colorAttribute = {};
			colorAttribute.binding = 0;
			colorAttribute.location = 2;
			colorAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
			colorAttribute.offset = offsetof(P3N3C3, color);

			description.attributes.push_back(positionAttribute);
			description.attributes.push_back(normalAttribute);
			description.attributes.push_back(colorAttribute);
			return description;
		}
	};

	struct P3N3C3U2 {
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec3 color;
		glm::vec2 uv;

		static constexpr VertexInputDescription get_vertex_description() {
			VertexInputDescription description;

			//we will have just 1 vertex buffer binding, with a per-vertex rate
			VkVertexInputBindingDescription mainBinding = {};
			mainBinding.binding = 0;
			mainBinding.stride = sizeof(P3N3C3U2);
			mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			description.bindings.push_back(mainBinding);

			//Position will be stored at Location 0
			VkVertexInputAttributeDescription positionAttribute = {};
			positionAttribute.binding = 0;
			positionAttribute.location = 0;
			positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
			positionAttribute.offset = offsetof(P3N3C3U2, pos);

			//Normal will be stored at Location 1
			VkVertexInputAttributeDescription normalAttribute = {};
			normalAttribute.binding = 0;
			normalAttribute.location = 1;
			normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
			normalAttribute.offset = offsetof(P3N3C3U2, normal);

			//Color will be stored at Location 2
			VkVertexInputAttributeDescription colorAttribute = {};
			colorAttribute.binding = 0;
			colorAttribute.location = 2;
			colorAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
			colorAttribute.offset = offsetof(P3N3C3U2, color);

			VkVertexInputAttributeDescription uvAttribute = {
				.location = 3,
				.binding = 0,
				.format = VK_FORMAT_R32G32_SFLOAT,
				.offset = offsetof(P3N3C3U2, uv),
			};

			description.attributes.push_back(positionAttribute);
			description.attributes.push_back(normalAttribute);
			description.attributes.push_back(colorAttribute);
			description.attributes.push_back(uvAttribute);
			return description;
		}
	};

	struct GPUCameraData {
		glm::mat4 view;
		glm::mat4 proj;
		glm::mat4 viewproj;
	};

	struct GPUSceneData {
		glm::vec4 fog_color;
		glm::vec4 fog_distances;
		glm::vec4 ambient_color;
		glm::vec4 sunlight_direction;
		glm::vec4 sunlight_color;
		GPUCameraData camera;
	};

	struct GPUMakeupData {
		glm::vec4 color;
	};

	
}