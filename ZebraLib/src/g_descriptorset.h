#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include "zebratypes.h"
#include <map>
#include <unordered_set>

#include "vki.h"

namespace zebra {
	const u32 DefaultFatSize = 8;

	struct SmallLayoutBinding {
		u32 binding;
		VkDescriptorType descriptorType;
		VkShaderStageFlags stageFlags;

		static constexpr VkDescriptorSetLayoutBinding vk_binding(SmallLayoutBinding small) {
			return vki::descriptorset_layout_binding(small.descriptorType, small.stageFlags, small.binding);
		}
	};

	template<u32 BufferSize>
	struct FatSetLayout {
		static constexpr int Size = BufferSize;

		VkDescriptorSetLayout layout = VK_NULL_HANDLE;
		u32 binding_count = 0;
		// must be sorted
		std::array<SmallLayoutBinding, Size> bindings;
	
		bool operator==(const FatSetLayout& other) const {
			if (binding_count != other.binding_count) return false;
			for (u32 i = 0; i < binding_count; i++) {
				if (bindings[i].binding != other.bindings[i].binding) return false;
				if (bindings[i].descriptorType != other.bindings[i].descriptorType) return false;
				if (bindings[i].stageFlags != other.bindings[i].stageFlags) return false;
			}
			return true;
		}

		size_t hash() const {
			auto result = std::hash<size_t>()(sizeof(bindings));
			for (u32 i = 0; i < binding_count; i++) {
				size_t binding_hash = bindings[i].binding | bindings[i].descriptorType << 8 | bindings[i].stageFlags << 32;
				result ^= std::hash<size_t>()(binding_hash);
			}
			return result;
		}
	};

	struct FatSetLayoutHasher {
		std::size_t operator()(const FatSetLayout<DefaultFatSize>& k) const {
			return k.hash();
		}
	};

	struct DescriptorLayoutCache {
		std::unordered_set<FatSetLayout<DefaultFatSize>, FatSetLayoutHasher> layouts;
		VkDescriptorSetLayout create(VkDevice device, FatSetLayout<DefaultFatSize>& layout);

	};

	struct DescriptorBuilder {
		FatSetLayout<DefaultFatSize> recipe{};
		std::array<VkWriteDescriptorSet, DefaultFatSize> writes;

		static DescriptorBuilder& begin(VkDevice device, VkDescriptorPool pool, DescriptorLayoutCache& cache);
		DescriptorBuilder& bind_buffer(u32 binding, VkDescriptorBufferInfo& buffer_info, VkDescriptorType type, VkShaderStageFlags stage_flags);
		void build(VkDescriptorSet& set);
		void build(VkDescriptorSet& set, VkDescriptorSetLayout& layout);


		// CONTINUE HERE, bob the builder

	protected: 
		DescriptorBuilder() = default;
		VkDevice device;
		VkDescriptorPool pool;
		DescriptorLayoutCache* cache;
	};
}