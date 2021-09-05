#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include "zebratypes.h"
#include <map>
#include <unordered_set>

#include "vki.h"

namespace zebra {
	struct SmallLayoutBinding {
		u32 binding;
		VkDescriptorType descriptorType;
		VkShaderStageFlags stageFlags;

		static constexpr VkDescriptorSetLayoutBinding vk_binding(SmallLayoutBinding small) {
			return vki::descriptorset_layout_binding(small.descriptorType, small.stageFlags, small.binding);
		}
	};

	struct FatSetLayout {
		static constexpr int Size = 8;

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
		size_t hash() const;
	};

	struct FatSetLayoutHasher {
		std::size_t operator()(const FatSetLayout& k) const {
			return k.hash();
		}
	};

	struct DescriptorLayoutCache {
		std::unordered_set<FatSetLayout, FatSetLayoutHasher> layouts;
		VkDescriptorSetLayout create(VkDevice device, FatSetLayout& layout);

	};

	struct DescriptorBuilder {
		FatSetLayout recipe;
		// CONTINUE HERE, bob the builder
	};
}