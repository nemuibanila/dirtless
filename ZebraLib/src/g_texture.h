#pragma once

#include "g_types.h"

namespace zebra {
	bool load_image_from_file(UploadContext& up, const char* file, VkImage& out_image, VmaAllocation& out_allocation);
	bool create_gpu_texture(zebra::UploadContext& up, VkImageCreateInfo& iinfo, VkImageAspectFlags aspects, zebra::Texture& tex);
	void destroy_texture(UploadContext& up, Texture tex);
};