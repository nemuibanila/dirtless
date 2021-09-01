#include "g_buffer.h"
#include "z_debug.h"

namespace zebra {
	AllocBuffer create_buffer(VmaAllocator& allocator, size_t alloc_size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage) {
		VkBufferCreateInfo buffer_info = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext = nullptr,
			.size = alloc_size,
			.usage = usage,
		};

		VmaAllocationCreateInfo vma_info = {
			.usage = memory_usage,
		};

		AllocBuffer buf{};
		VK_CHECK(vmaCreateBuffer(allocator, &buffer_info, &vma_info, &buf.buffer, &buf.allocation, nullptr));
		return buf;
	}


}