#include "g_pipeline.h"
#include "zebratypes.h"

namespace zebra {
	VkPipeline PipelineBuilder::build_pipeline(VkDevice device, VkRenderPass pass) {
		//make viewport state from our stored viewport and scissor.
		//at the moment we won't support multiple viewports or scissors
		VkPipelineViewportStateCreateInfo viewport_state = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.pNext = nullptr,
			.viewportCount = 1,
			.pViewports = &_viewport,
			.scissorCount = 1,
			.pScissors = &_scissor,
		};

		//setup dummy color blending. We aren't using transparent objects yet
		//the blending is just "no blend", but we do write to the color attachment
		VkPipelineColorBlendStateCreateInfo color_blending = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.pNext = nullptr,
			.logicOpEnable = VK_FALSE,
			.logicOp = VK_LOGIC_OP_COPY,
			.attachmentCount = 1,
			.pAttachments = &_color_blend_attachment,
		};

		VkGraphicsPipelineCreateInfo pipeline_info = {
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext = nullptr,
			.stageCount = (u32)_shader_stages.size(),
			.pStages = _shader_stages.data(),
			.pVertexInputState = &_vertex_input_info,
			.pInputAssemblyState = &_input_assembly,
			.pViewportState = &viewport_state,
			.pRasterizationState = &_rasterizer,
			.pMultisampleState = &_multisampling,
			.pDepthStencilState = &_depth_stencil,
			.pColorBlendState = &color_blending,
			.layout = _pipelineLayout,
			.renderPass = pass,
			.subpass = 0,
			.basePipelineHandle = VK_NULL_HANDLE,
		};

		VkPipeline new_pipeline;
		auto result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &new_pipeline);

		if (result != VK_SUCCESS) {
			return VK_NULL_HANDLE;
		} else {
			return new_pipeline;
		}
	}
}