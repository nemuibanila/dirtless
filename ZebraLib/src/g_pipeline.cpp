#include "g_pipeline.h"
#include "zebratypes.h"
#include "vki.h"

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

		auto dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

		VkPipelineDynamicStateCreateInfo dynamic_info = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.pNext = nullptr,
			.dynamicStateCount = (u32) dynamic_states.size(),
			.pDynamicStates = dynamic_states.begin(),
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
			.pDynamicState = &dynamic_info,
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
	/// @brief Sets up for filled polygons in a triangle list
	/// @return this
	PipelineBuilder& PipelineBuilder::set_defaults() {
		auto& pipeline_builder = *this;

		pipeline_builder._vertex_input_info = vki::vertex_input_state_create_info();
		pipeline_builder._input_assembly = vki::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
		pipeline_builder._rasterizer = vki::rasterization_state_create_info(VK_POLYGON_MODE_FILL);
		pipeline_builder._multisampling = vki::multisampling_state_create_info();
		pipeline_builder._color_blend_attachment = vki::color_blend_attachment_state();
		return pipeline_builder;
	}

	PipelineBuilder& PipelineBuilder::set_vertex_format(VertexInputDescription& description) {
		auto& pipeline_builder = *this;
		pipeline_builder._vertex_input_info.vertexAttributeDescriptionCount = (u32)description.attributes.size();
		pipeline_builder._vertex_input_info.pVertexAttributeDescriptions = description.attributes.data();
		pipeline_builder._vertex_input_info.vertexBindingDescriptionCount = (u32)description.bindings.size();
		pipeline_builder._vertex_input_info.pVertexBindingDescriptions = description.bindings.data();
		return pipeline_builder;
	}

	PipelineBuilder& PipelineBuilder::no_vertex_format() {
		auto& pipeline_builder = *this;
		pipeline_builder._vertex_input_info.vertexAttributeDescriptionCount = 0;
		pipeline_builder._vertex_input_info.pVertexAttributeDescriptions = nullptr;
		pipeline_builder._vertex_input_info.vertexBindingDescriptionCount = 0;
		pipeline_builder._vertex_input_info.pVertexBindingDescriptions = nullptr;
		return pipeline_builder;
	}

	PipelineBuilder& PipelineBuilder::add_shader(VkShaderStageFlagBits stage, VkShaderModule shader) {
		auto& pipeline_builder = *this;
		pipeline_builder._shader_stages.push_back(vki::pipeline_shader_stage_create_info(stage, shader));
		return pipeline_builder;
	}
}