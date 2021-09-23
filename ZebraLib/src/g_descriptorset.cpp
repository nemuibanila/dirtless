#include "g_descriptorset.h"
#include <functional>
#include <vulkan/vulkan.h>
#include "vki.h"
#include "z_debug.h"

VkDescriptorSetLayout zebra::DescriptorLayoutCache::create(VkDevice device, FatSetLayout<DefaultFatSize>& layout) {
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

		VK_CHECK(vkCreateDescriptorSetLayout(device, &cinfo, nullptr, &layout.layout));
		layouts.emplace(layout);
		return layout.layout;
	}
}


void zebra::DescriptorLayoutCache::destroy_cached(VkDevice device) {
	for (auto& v : layouts) {
		if (v.layout == VK_NULL_HANDLE) continue;
		vkDestroyDescriptorSetLayout(device, v.layout, nullptr);
	}

	layouts.clear();
}

zebra::DescriptorBuilder zebra::DescriptorBuilder::begin(VkDevice device, VkDescriptorPool pool, zebra::DescriptorLayoutCache& cache) {
	zebra::DescriptorBuilder builder{};
	builder.pool = pool;
	builder.device = device;
	builder.cache = &cache;
	return builder;
}


zebra::DescriptorBuilder& zebra::DescriptorBuilder::bind_buffer(u32 binding, VkDescriptorBufferInfo& buffer_info, VkDescriptorType type, VkShaderStageFlags stage_flags) {
	SmallLayoutBinding smol = {
		.binding = binding,
		.descriptorType = type,
		.stageFlags = stage_flags,
	};
	auto idx = recipe.binding_count++;
	recipe.bindings[idx] = smol;
	writes[idx] = vki::write_descriptor_buffer(type, VK_NULL_HANDLE, &buffer_info, binding);

	return *this;
}

zebra::DescriptorBuilder& zebra::DescriptorBuilder::bind_image(u32 binding, VkDescriptorImageInfo& image_info, VkDescriptorType type, VkShaderStageFlags stage_flags) {
	SmallLayoutBinding smol = {
		.binding = binding,
		.descriptorType = type,
		.stageFlags = stage_flags,
	};
	auto idx = recipe.binding_count++;
	recipe.bindings[idx] = smol;
	writes[idx] = vki::write_descriptor_image(type, VK_NULL_HANDLE, &image_info, binding);

	return *this;
}

void zebra::DescriptorBuilder::build(VkDescriptorSet& set, VkDescriptorSetLayout& layout) {
	layout = cache->create(device, recipe);
	
	// defer to build step
	VkDescriptorSetAllocateInfo dinfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &layout, // data dependency fail
	};

	VK_CHECK(vkAllocateDescriptorSets(device, &dinfo, &set));
	for (auto i = 0u; i < recipe.binding_count; i++) {
		writes[i].dstSet = set;
	}

	vkUpdateDescriptorSets(device, recipe.binding_count, writes.data(), 0, nullptr);
	// end defer
}


void zebra::DescriptorBuilder::build(VkDescriptorSet& set) {
	VkDescriptorSetLayout layout;
	zebra::DescriptorBuilder::build(set, layout);
}