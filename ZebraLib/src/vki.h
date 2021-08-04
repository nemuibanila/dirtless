#pragma once
#include <vulkan/vulkan.h>

namespace vki {
	VkCommandPoolCreateInfo command_pool_create_info(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0) {
		return VkCommandPoolCreateInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = flags,
			.queueFamilyIndex = queueFamilyIndex,
		};
	}

	VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool pool, u32 count = 1, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) {
		return VkCommandBufferAllocateInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext = nullptr,
			.commandPool = pool,
			.level = level,
			.commandBufferCount = count,
		};
	}
}