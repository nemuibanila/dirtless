#include "renderer.h"
#include "g_buffer.h"
#include "d_rel.h"
#include "vki.h"
#include "g_descriptorset.h"
#include "z_debug.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <algorithm>
#include <functional>

namespace zebra {
	namespace render {
		u64 insert_mesh(Assets& assets, Mesh mesh) {
			auto k = genkey();
			assets.t_meshes[k] = mesh;
			return k;
		}
		u64 insert_material(Assets& assets, Material material) {
			auto k = genkey();
			assets.t_materials[k] = material;
			return k;
		}

		u64 insert_texture(Assets& assets, Texture texture) {
			auto k = genkey();
			assets.t_textures[k] = texture;
			return k;
		}

		void name_handle(Assets& assets, std::string name, u64 handle) {
			assets.t_names[name] = handle;
		}
		
		VkRenderPass RenderPassCache::get_or_create(std::span<DependencyInfo> info) {
			if (cache.find(info) == cache.end()) [[unlikely]] {
				// fill cache
				std::vector<VkAttachmentDescription> attachment_descs(info.size());
				std::vector<VkAttachmentReference> attachment_references(info.size());
				std::vector<VkAttachmentReference> color_attachment_refs(info.size());
				std::vector<VkAttachmentReference> depth_attachment_refs(1);
				
				for (DependencyInfo di : info) {
					attachment_descs.emplace_back(
						vki::attachment_description(
							di.format,
							di.initial_layout,
							di.final_layout,
							di.on_load,
							di.on_store
						)
					);
					
					auto attachment_ref = VkAttachmentReference {
							.attachment = attachment_references.size(),
							.layout = di.attachment_layout
						};
					
					attachment_references.emplace_back(
						attachment_ref
					);
					
					switch(di.attachment_layout) {
						case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
							color_attachment_refs.emplace_back(attachment_ref);
							break;
						case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
							depth_attachment_refs.emplace_back(attachment_ref);
							if (depth_attachment_refs.size() > 1) [[unlikely]] {
								DBG("more than one depth attachment. this is not supported, but we will let it slide.. however, you should change this.");
							}
							
							break;
						[[unlikely]] default:
							DBG("invalid attachment layout passed. you probably didnt want this! layout: " << attachment_ref.layout << " idx: " << attachment_ref.attachment);
							abort();
							
					}
				}
				
				VkSubpassDescription subpass = {
					.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
					.colorAttachmentCount = color_attachment_refs.size(),
					.pColorAttachments = color_attachment_refs.data(),
					.pDepthStencilAttachment = depth_attachment_refs.data(),
				};
				
				VkRenderPassCreateInfo render_pass_info = {
					.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
					.attachmentCount = (u32)attachment_descs.size(),
					.pAttachments = attachment_descs.data(),
					.subpassCount = 1,
					.pSubpasses = &subpass
				};
				VkRenderPass uwu_finally;
				VK_CHECK(vkCreateRenderPass(this->device, &render_pass_info, nullptr, &uwu_finally));
				
				cache[info] = uwu_finally;
				return uwu_finally;
				
			} else [[likely]] {
				return cache[info];
			}
		}

		
		void add_renderable(Renderer& renderer, RenderObject object, bool bStatic) {
			if (bStatic) {
				renderer.t_statics.push_back(object);
				renderer.b_statics_sorted = false;
			} else {
				renderer.t_objects.push_back(object);
			}
		}

		void begin_collect(Renderer& renderer, UploadContext& up) {
			renderer.t_objects.resize(0);
			renderer.up = &up;

			// wait for unused?
			for (auto& buffer : renderer.used_buffers) {
				renderer.available_buffers.push_back(buffer.buffer);
			}
			
			renderer.used_buffers.clear();

			// dangerous buffers that are used per frame
			for (auto i = renderer.danger_buffers.begin(); i != renderer.danger_buffers.end();) {
				if (vkGetFenceStatus(up.device, (*i).fence_usage) == VK_SUCCESS) {
					renderer.available_buffers.push_back((*i).buffer);
					i = renderer.danger_buffers.erase(i);
				} else {
					i += 1;
				}
			}
			
		}

		AllocBuffer pop_buffer(Renderer& renderer, bool bJustTake) {
			AllocBuffer buffer;
			if (renderer.available_buffers.empty()) {
				buffer = create_buffer(renderer.up->allocator, SINGLE_BUFFER_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
			} else {
				buffer = renderer.available_buffers.front();
				renderer.available_buffers.pop_front();
			}
			if (!bJustTake) {
				renderer.used_buffers.push_back({ buffer });
			}
			return buffer;
		}

		AllocBuffer pop_hot_buffer(Renderer& renderer, VkFence render_fence) {
			AllocBuffer buffer;
			buffer = pop_buffer(renderer, true);
			DangerBuffer danger;
			danger.fence_usage = render_fence;
			danger.buffer = buffer;
			renderer.danger_buffers.push_back(danger);
			return buffer;
		}

		void finish_collect(Renderer& renderer) {
			// frustrum culling
			std::sort(renderer.t_objects.begin(), renderer.t_objects.end(), [](const RenderObject& a, const RenderObject& b) {
				return a.material_fk > b.material_fk || a.material_fk == b.material_fk && a.mesh_fk > b.mesh_fk;
				});

			if (!renderer.b_statics_sorted) {
				std::sort(renderer.t_statics.begin(), renderer.t_statics.end(), [](const RenderObject& a, const RenderObject& b) {
					return a.material_fk > b.material_fk || a.material_fk == b.material_fk && a.mesh_fk > b.mesh_fk;
					}); 
				renderer.b_statics_sorted = true;
			}
		}

		constexpr VkClearValue clear_color = { .color = {0.2f, 0.2f, 1.f, 1.f} };
		constexpr VkClearValue clear_depth = { .depthStencil = {1.f, 0 }};
		void render(Renderer& renderer, Assets& assets, PerFrameData& frame, UploadContext& up, GPUSceneData& params, RenderData& rdata) {
			// -- Data dependencies
			VkRenderPass& forward_pass = rdata.forward_pass;
			VkFramebuffer& forward_framebuffer = rdata.forward_framebuffer;
			VkExtent2D& forward_extent = rdata.forward_extent;
			DescriptorLayoutCache& dcache = *rdata.dcache;

			// -- Data dependencies end


			// SHADOW PASS
			// ...
			// ...

			// FORWARD PASS
			auto clear_values = { clear_color, clear_depth };
			auto renderpass_info = vki::renderpass_begin_info(forward_pass, forward_framebuffer, forward_extent);
			renderpass_info.clearValueCount = 2;
			renderpass_info.pClearValues = clear_values.begin();

			vkCmdBeginRenderPass(frame.buf, &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);
			auto viewport = vki::viewport_info(forward_extent);
			vkCmdSetViewport(frame.buf, 0, 1, &viewport);
			VkRect2D scissor = { .offset = { 0, 0 }, .extent = forward_extent};
			vkCmdSetScissor(frame.buf, 0, 1, &scissor);

			// -- setup scene
			auto scene_param_buffer = pop_hot_buffer(renderer, frame.renderF);

			VkDescriptorBufferInfo sinfo = {
				.buffer = scene_param_buffer.buffer,
				.offset = 0,
				.range = sizeof(GPUSceneData),
			};

			VkDescriptorSet scene_set;
			DescriptorBuilder::begin(up.device, frame.descriptor_pool, dcache)
				.bind_buffer(0, sinfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
				.build(scene_set);

			{ // CPUTOGPU -- scene buffer copy
				MappedBuffer<GPUSceneData> scene_map{ up.allocator, scene_param_buffer };
				*scene_map.data = params;
			}

			//
			draw_batches(renderer, up, frame, dcache, renderer.t_statics, assets, scene_set, true);
			draw_batches(renderer, up, frame, dcache, renderer.t_objects, assets, scene_set);
			// POSTPROCESS PASS
		}

		void clear_buffers(zebra::render::Renderer& renderer, zebra::UploadContext& up) {
			for (auto& buf : renderer.static_buffers) {
				vmaDestroyBuffer(up.allocator, buf.buffer, buf.allocation);

			}
			renderer.static_buffers.clear();
			for (auto& buf : renderer.danger_buffers) {
				vmaDestroyBuffer(up.allocator, buf.buffer.buffer, buf.buffer.allocation);
			}
			renderer.danger_buffers.clear();

			for (auto& buf : renderer.used_buffers) {
				vmaDestroyBuffer(up.allocator, buf.buffer.buffer, buf.buffer.allocation);
			}
			for (auto& buf : renderer.available_buffers) {
				vmaDestroyBuffer(up.allocator, buf.buffer, buf.allocation);
			}

		}

		void draw_batches(zebra::render::Renderer& renderer, zebra::UploadContext& up, zebra::PerFrameData& frame, zebra::DescriptorLayoutCache& dcache, std::vector<zebra::render::RenderObject>& object_vector, zebra::render::Assets& assets, VkDescriptorSet& scene_set, bool bStatic) {
			const u64 BATCH_SIZE = (SINGLE_BUFFER_SIZE / (sizeof(GPUObjectData) + sizeof(VkDrawIndirectCommand)));
			const u64 DRAW_OFFSET = BATCH_SIZE * sizeof(GPUObjectData);

			std::vector<StaticDrawInfo> scratch_draws;
			scratch_draws.reserve(16);

			for (auto ridx = 0ull;; ridx += BATCH_SIZE) {
				// -- start object buffer
				auto left = std::min(BATCH_SIZE, static_cast<u64>(object_vector.size() - ridx));
				auto object_buffer = pop_hot_buffer(renderer, frame.renderF);
				MappedBuffer<GPUObjectData> object_map{ up.allocator, object_buffer };
				for (auto i = 0ull; i < left; i++) {
					object_map[i] = object_vector[ridx + i].obj;
				}
				// -- end object buffer
				
				// -- start descriptor
				VkDescriptorBufferInfo oinfo = {
					.buffer = object_buffer.buffer,
					.offset = 0,
					.range = DRAW_OFFSET,
				};

				VkDescriptorSet object_set;
				DescriptorBuilder::begin(up.device, frame.descriptor_pool, dcache)
					.bind_buffer(0, oinfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
					.build(object_set);
				// -- end descriptor

				// -- start draw command generator
				auto batch_id = 0u;
				VkDrawIndirectCommand* draw_ptr = (VkDrawIndirectCommand*)(((u8*)object_map.data) + DRAW_OFFSET);
				for (auto draw_i = 0ull; batch_id < left; draw_i += 1) {
					auto& prototype = object_vector[ridx + batch_id];
					auto& material = assets.t_materials[prototype.material_fk];
					auto& mesh = assets.t_meshes[prototype.mesh_fk];

					VkDrawIndirectCommand draw_command = {
						.vertexCount = (u32)assets.t_meshes[object_vector[ridx + batch_id].mesh_fk].size,
						.instanceCount = 0,
						.firstVertex = 0,
						.firstInstance = batch_id,
					};

					do {
						draw_command.instanceCount += 1;
						batch_id += 1;
					} while (
						batch_id < left &&
						object_vector[ridx + batch_id].mesh_fk == prototype.mesh_fk &&
						object_vector[ridx + batch_id].material_fk == prototype.material_fk);

					draw_ptr[draw_i] = draw_command;
					vkCmdBindPipeline(frame.buf, VK_PIPELINE_BIND_POINT_GRAPHICS, material.pipeline);
					vkCmdBindDescriptorSets(frame.buf, VK_PIPELINE_BIND_POINT_GRAPHICS, material.pipeline_layout, 0, 1, &scene_set, 0, nullptr);
					vkCmdBindDescriptorSets(frame.buf, VK_PIPELINE_BIND_POINT_GRAPHICS, material.pipeline_layout, 1, 1, &object_set, 0, nullptr);
					if (material.texture_set != VK_NULL_HANDLE) {
						vkCmdBindDescriptorSets(frame.buf, VK_PIPELINE_BIND_POINT_GRAPHICS, material.pipeline_layout, 2, 1, &material.texture_set, 0, nullptr);
					}

					
					VkDeviceSize vertex_offset = 0;
					VkDeviceSize indirect_offset = DRAW_OFFSET + draw_i * sizeof(VkDrawIndirectCommand);
					vkCmdBindVertexBuffers(frame.buf, 0, 1, &mesh.vertices.buffer, &vertex_offset);

					vkCmdDrawIndirect(frame.buf, object_buffer.buffer, indirect_offset, 1, sizeof(VkDrawIndirectCommand));
				}
				// -- end draw command generator


				if (left < BATCH_SIZE) {
					break;
				}
			}
		}
	}
}