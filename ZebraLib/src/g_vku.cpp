#include "g_vku.h"
#include <vulkan/vulkan.h>
#include <functional>
#include "vki.h"

void vku::vk_immediate(zebra::UploadContext& up, std::function<void(VkCommandBuffer cmd)>&& function) {
	// update for seperate context
	VkCommandBufferAllocateInfo cmd_alloc_info = vki::command_buffer_allocate_info(up.pool);
	VkCommandBuffer cmd;
	vkAllocateCommandBuffers(up.device, &cmd_alloc_info, &cmd);
	auto begin_info = vki::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	vkBeginCommandBuffer(cmd, &begin_info);
	function(cmd);
	vkEndCommandBuffer(cmd);
	VkSubmitInfo submit = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = nullptr,
		.waitSemaphoreCount = 0,
		.pWaitSemaphores = nullptr,
		.pWaitDstStageMask = 0,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmd,
		.signalSemaphoreCount = 0,
		.pSignalSemaphores = nullptr,
	};

	vkQueueSubmit(up.graphics_queue, 1, &submit, up.uploadF);
	vkWaitForFences(up.device, 1, &up.uploadF, true, 999'999'999'999);
	vkResetFences(up.device, 1, &up.uploadF);

	vkResetCommandPool(up.device, up.pool, 0);
}