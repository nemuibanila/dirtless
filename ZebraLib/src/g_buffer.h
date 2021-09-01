#pragma once
#include <vk_mem_alloc.h>


namespace zebra {
	struct AllocBuffer {
		VkBuffer buffer;
		VmaAllocation allocation;
	};


	template<class T>
	struct MappedBuffer {
		T* data;
		VmaAllocator allocator;
		VmaAllocation allocation;

		MappedBuffer(const MappedBuffer&) = delete;
		void operator=(MappedBuffer const&) = delete;

		MappedBuffer(VmaAllocator& allocator, AllocBuffer& buffer) {
			this->allocator = allocator;
			this->allocation = buffer.allocation;
			void* ptr;
			vmaMapMemory(allocator, buffer.allocation, &ptr);
			this->data = (T*)ptr;
		}

		~MappedBuffer() {
			vmaUnmapMemory(allocator, allocation);
		}

		T& operator[](size_t idx) {
			return data[idx];
		}

		const T& operator[](size_t idx) const {
			return data[idx];
		}
	};


	AllocBuffer create_buffer(VmaAllocator& allocator, size_t alloc_size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage);
}