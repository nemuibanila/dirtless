#pragma once
#include <vector>

#include <vulkan/vulkan.h>
#include "g_types.h"
#include "vki.h"

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
		PipelineBuilder& add_shader(VkShaderStageFlagBits stage, VkShaderModule shader);
		PipelineBuilder& clear_shaders() {
			this->_shader_stages.clear();
			return *this;
		}
		PipelineBuilder& set_layout(VkPipelineLayout layout) {
			this->_pipelineLayout = layout;
			return *this;
		}
		PipelineBuilder& depth(bool depth_test, bool depth_write, VkCompareOp compare_op) {
			this->_depth_stencil = vki::depth_stencil_create_info(depth_test, depth_write, compare_op);
			return *this;
		}

		VkPipeline build_pipeline(VkDevice device, VkRenderPass pass);
	};
}