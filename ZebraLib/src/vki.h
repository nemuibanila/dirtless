﻿#pragma once
#include <vulkan/vulkan.h>
#include "zebratypes.h"

namespace vki {
	constexpr VkCommandPoolCreateInfo command_pool_create_info(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0) {
		return VkCommandPoolCreateInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = flags,
			.queueFamilyIndex = queueFamilyIndex,
		};
	}

	constexpr VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool pool, u32 count = 1, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) {
		return VkCommandBufferAllocateInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext = nullptr,
			.commandPool = pool,
			.level = level,
			.commandBufferCount = count,
		};
	}

	constexpr VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(VkShaderStageFlagBits stage, VkShaderModule shaderModule) {
		VkPipelineShaderStageCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		info.pNext = nullptr;

		info.stage = stage;
		info.module = shaderModule;
		info.pName = "main";
		return info;
	}

	constexpr VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info(VkPrimitiveTopology topology) {
		VkPipelineInputAssemblyStateCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		info.pNext = nullptr;

		info.topology = topology;
		//we are not going to use primitive restart on the entire tutorial so leave it on false
		info.primitiveRestartEnable = VK_FALSE;
		return info;
	}

	constexpr VkPipelineRasterizationStateCreateInfo rasterization_state_create_info(VkPolygonMode polygonMode) {
		VkPipelineRasterizationStateCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		info.pNext = nullptr;

		info.depthClampEnable = VK_FALSE;
		//discards all primitives before the rasterization stage if enabled which we don't want
		info.rasterizerDiscardEnable = VK_FALSE;

		info.polygonMode = polygonMode;
		info.lineWidth = 1.0f;
		//no backface cull
		info.cullMode = VK_CULL_MODE_NONE;
		info.frontFace = VK_FRONT_FACE_CLOCKWISE;
		//no depth bias
		info.depthBiasEnable = VK_FALSE;
		info.depthBiasConstantFactor = 0.0f;
		info.depthBiasClamp = 0.0f;
		info.depthBiasSlopeFactor = 0.0f;

		return info;
	}

	constexpr VkPipelineMultisampleStateCreateInfo multisampling_state_create_info() {
		VkPipelineMultisampleStateCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		info.pNext = nullptr;

		info.sampleShadingEnable = VK_FALSE;
		//multisampling defaulted to no multisampling (1 sample per pixel)
		info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		info.minSampleShading = 1.0f;
		info.pSampleMask = nullptr;
		info.alphaToCoverageEnable = VK_FALSE;
		info.alphaToOneEnable = VK_FALSE;
		return info;
	}

	constexpr VkPipelineColorBlendAttachmentState color_blend_attachment_state() {
		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;
		return colorBlendAttachment;
	}

	constexpr VkPipelineLayoutCreateInfo pipeline_layout_create_info() {
		VkPipelineLayoutCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.setLayoutCount = 0,
			.pSetLayouts = nullptr,
			.pushConstantRangeCount = 0,
			.pPushConstantRanges = nullptr,
		};
		return create_info;
	}

	constexpr VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info() {
		VkPipelineVertexInputStateCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		info.pNext = nullptr;

		//no vertex bindings or attributes
		info.vertexBindingDescriptionCount = 0;
		info.vertexAttributeDescriptionCount = 0;
		return info;
	}

	constexpr VkImageCreateInfo image_create_info(VkFormat format, VkImageUsageFlags usage_flags, VkExtent3D extent) {
		VkImageCreateInfo info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.pNext = nullptr,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = format,
			.extent = extent,
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.tiling = VK_IMAGE_TILING_OPTIMAL,
			.usage = usage_flags,
		};

		return info;
	}

	constexpr VkImageViewCreateInfo imageview_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspect_flags) {
		VkImageViewCreateInfo info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = nullptr,
			.image = image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = format,
			.subresourceRange = {
				.aspectMask = aspect_flags,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};

		return info;
	}

	constexpr VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info(bool bdepth_test, bool bdepth_write, VkCompareOp compare_op) {
		VkPipelineDepthStencilStateCreateInfo info = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.pNext = nullptr,
			.depthTestEnable = bdepth_test ? VK_TRUE : VK_FALSE,
			.depthWriteEnable = bdepth_write ? VK_TRUE : VK_FALSE,
			.depthCompareOp = bdepth_test ? compare_op : VK_COMPARE_OP_ALWAYS,
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable = VK_FALSE,
			.minDepthBounds = 0.0f,
			.maxDepthBounds = 1.0f,
		};

		return info;
	}

	constexpr VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags) {
		return {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = nullptr,
			.flags = flags,
			.pInheritanceInfo = nullptr,
		};
	}

	constexpr VkDescriptorSetLayoutBinding descriptorset_layout_binding(VkDescriptorType type, VkShaderStageFlags stageFlags, u32 binding) {
		VkDescriptorSetLayoutBinding setbind = {};
		setbind.binding = binding;
		setbind.descriptorCount = 1;
		setbind.descriptorType = type;
		setbind.pImmutableSamplers = nullptr;
		setbind.stageFlags = stageFlags;

		return setbind;
	}

	constexpr VkWriteDescriptorSet write_descriptor_buffer(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorBufferInfo* bufferInfo, u32 binding) {
		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.pNext = nullptr;

		write.dstBinding = binding;
		write.dstSet = dstSet;
		write.descriptorCount = 1;
		write.descriptorType = type;
		write.pBufferInfo = bufferInfo;

		return write;
	}

	constexpr VkSamplerCreateInfo sampler_create_info(VkFilter filters, VkSamplerAddressMode sampler_address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT) {
		VkSamplerCreateInfo info = {
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.pNext = nullptr,
			.magFilter = filters,
			.minFilter = filters,
			.addressModeU = sampler_address_mode,
			.addressModeV = sampler_address_mode,
			.addressModeW = sampler_address_mode,
		};
		return info;
	}

	constexpr VkWriteDescriptorSet write_descriptor_image(VkDescriptorType type, VkDescriptorSet dest_set, VkDescriptorImageInfo* image_info, uint32_t binding) {
		VkWriteDescriptorSet write = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = dest_set,
			.dstBinding = binding,
			.descriptorCount = 1,
			.descriptorType = type,
			.pImageInfo = image_info,
		};
		return write;
	}

	constexpr VkAttachmentDescription attachment_description(VkFormat format,
		VkImageLayout initial_layout,
		VkImageLayout final_layout,
		VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
		VkAttachmentStoreOp store_op = VK_ATTACHMENT_STORE_OP_STORE) {
		return VkAttachmentDescription{
			.format = format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = load_op,
			.storeOp = store_op,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = initial_layout,
			.finalLayout = final_layout,
		};
	}

	constexpr VkFramebufferCreateInfo framebuffer_info(VkRenderPass renderpass, VkExtent2D extent) {
		VkFramebufferCreateInfo fb_info = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.renderPass = renderpass,
			.attachmentCount = 0,
			.pAttachments = nullptr,
			.width = extent.width,
			.height = extent.height,
			.layers = 1,
		};
		return fb_info;
	}

	constexpr VkRenderPassBeginInfo renderpass_begin_info(VkRenderPass renderpass, VkFramebuffer framebuffer, VkExtent2D extent) {
		return VkRenderPassBeginInfo{
		   .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		   .pNext = nullptr,
		   .renderPass = renderpass,
		   .framebuffer = framebuffer,
		   .renderArea = {
			   .offset = {
			   .x = 0,
			   .y = 0,
	   },
	   .extent = extent,
	   },
	   .clearValueCount = 0,
	   .pClearValues = nullptr,
		};
	}

	constexpr VkViewport viewport_info(VkExtent2D extent) {
		VkViewport viewport = {};
		viewport.height = (float)extent.height;
		viewport.width = (float)extent.width;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		viewport.x = 0;
		viewport.y = 0;
		return viewport;
	}
}

