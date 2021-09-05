#include "g_descriptorset.h"
#include <functional>
#include <vulkan/vulkan.h>
#include "vki.h"

size_t zebra::FatSetLayout::hash() const {
	auto result = std::hash<size_t>()(sizeof(bindings));
	for (u32 i = 0; i < binding_count; i++) {
		size_t binding_hash = bindings[i].binding | bindings[i].descriptorType << 8 | bindings[i].stageFlags << 32;
		result ^= std::hash<size_t>()(binding_hash);
	}
	return result;
}

VkDescriptorSetLayout zebra::DescriptorLayoutCache::create(VkDevice device, FatSetLayout& layout) {
	auto it = layouts.find(layout);
	if (it != layouts.end()) {
		return it->layout;
	} else {
		std::vector<VkDescriptorSetLayoutBinding> fat_bindings{ layout.binding_count };
		for (auto i = 0u; i < layout.binding_count; i++) {
			fat_bindings[i] = SmallLayoutBinding::vk_binding(layout.bindings[i]);
		}

		VkDescriptorSetLayoutCreateInfo cinfo = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.bindingCount = (u32)fat_bindings.size(),
			.pBindings = fat_bindings.data(),
		};

		vkCreateDescriptorSetLayout(device, &cinfo, nullptr, &layout.layout);
		layouts.emplace(layout);
		return layout.layout;
	}
}