#include "g_texture.h"

#include <iostream>
#include "vki.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

bool vku::load_image_from_file(zebra::UploadContext& up, const char* file, zebra::AllocImage& out_image) {
	int tw, th, tc;

	stbi_uc* pixels = stbi_load(file, &tw, &th, &tc, STBI_rgb_alpha);
	if (!pixels) {
		DBG("Failed to load texture file. " << file);
		return false;
	}
	
	VkDeviceSize image_size = (u64)tw * (u64)th * 4u;

	VkFormat image_format = VK_FORMAT_R8G8B8A8_SRGB;
	zebra::AllocBuffer staging_buffer = zebra::create_buffer(up.allocator, image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	void* data;
	vmaMapMemory(up.allocator, staging_buffer.allocation, &data);
	memcpy(data, pixels, image_size);
	vmaUnmapMemory(up.allocator, staging_buffer.allocation);

	stbi_image_free(pixels);
	// stuffs is now in staging buffer

	VkExtent3D image_extent = {
		.width = (u32)tw,
		.height = (u32)th,
		.depth = 1,
	};

	VkImageCreateInfo dimg_info = vki::image_create_info(image_format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, image_extent);
	zebra::AllocImage gpu_image;
	VmaAllocationCreateInfo dimg_allocinfo = {
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
	};

	vmaCreateImage(up.allocator, &dimg_info, &dimg_allocinfo, &gpu_image.image, &gpu_image.allocation, nullptr);
	zebra::vk_immediate(up, [=](VkCommandBuffer cmd) {
		VkImageSubresourceRange range = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		};

		VkImageMemoryBarrier imageBarrier_toTransfer = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = 0,
			.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.image = gpu_image.image,
			.subresourceRange = range,
		};

		vkCmdPipelineBarrier(cmd, 
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 0, &imageBarrier_toTransfer);

		VkBufferImageCopy copy_region = {
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = 0,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
			.imageExtent = image_extent,
		};

		vkCmdCopyBufferToImage(cmd, staging_buffer.buffer, gpu_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

		VkImageMemoryBarrier imageBarrier_toReadable = imageBarrier_toTransfer;
		imageBarrier_toReadable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBarrier_toReadable.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		imageBarrier_toReadable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageBarrier_toReadable.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(cmd, 
			VK_PIPELINE_STAGE_TRANSFER_BIT, 
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier_toReadable); 
		});

	vmaDestroyBuffer(up.allocator, staging_buffer.buffer, staging_buffer.allocation);

	DBG("Texture loaded sucessfully: " << file);
	out_image = gpu_image;
	return true;
}
