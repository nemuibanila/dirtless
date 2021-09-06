#pragma once
#include <vector>

#include <vulkan/vulkan.h>
#include "g_types.h"

namespace zebra {
	class PipelineBuilder {
	public:
		std::vector<VkPipelineShaderStageCreateInfo> _shader_stages;
		VkPipelineVertexInputStateCreateInfo _vertex_input_info;
		VkPipelineInputAssemblyStateCreateInfo _input_assembly;
		VkViewport _viewport;
		VkRect2D _scissor;
		VkPipelineRasterizationStateCreateInfo _rasterizer;
		VkPipelineColorBlendAttachmentState _color_blend_attachment;
		VkPipelineDepthStencilStateCreateInfo _depth_stencil;
		VkPipelineMultisampleStateCreateInfo _multisampling;
		VkPipelineLayout _pipelineLayout;

		PipelineBuilder& set_defaults();
		PipelineBuilder& set_vertex_format(VertexInputDescription& description);
		PipelineBuilder& no_vertex_format();
		VkPipeline build_pipeline(VkDevice device, VkRenderPass pass);
	};
}