#pragma once

#include "g_types.h"
#include <functional>
#include <vulkan/vulkan.h>

namespace vku {
	void vk_immediate(zebra::UploadContext& up, std::function<void(VkCommandBuffer cmd)>&& function);
}