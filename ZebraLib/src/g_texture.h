#pragma once

#include "g_types.h"

namespace zebra {
	bool load_image_from_file(UploadContext& up, const char* file, zebra::AllocImage& out_image);
	bool create_gpu_texture(zebra::UploadContext& up, VkImageCreateInfo& iinfo, VkImageAspectFlags aspects, zebra::Texture& tex);
	void destroy_texture(UploadContext& up, Texture tex);
};