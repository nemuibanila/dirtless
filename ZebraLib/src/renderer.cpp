#include "renderer.h"
#include "g_buffer.h"
#include "d_rel.h"
#include "vki.h"
#include "g_descriptorset.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <algorithm>

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
		}

		AllocBuffer pop_buffer(Renderer& renderer) {
			AllocBuffer buffer;
			if (renderer.available_buffers.empty()) {
				const u32 SINGLE_BUFFER_SIZE = 65536;
				buffer = create_buffer(renderer.up->allocator, SINGLE_BUFFER_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
			} else {
				buffer = renderer.available_buffers.front();
				renderer.available_buffers.pop_front();
			}
			renderer.used_buffers.push_back({ buffer });
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
		constexpr VkClearValue clear_depth = { .depthStencil = 1.f };
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
			auto scene_param_buffer = pop_buffer(renderer);

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
			draw_batches(renderer, up, frame, dcache, renderer.t_statics, assets, scene_set);
			draw_batches(renderer, up, frame, dcache, renderer.t_objects, assets, scene_set);
			
			// POSTPROCESS PASS
		}

		void draw_batches(zebra::render::Renderer& renderer, zebra::UploadContext& up, zebra::PerFrameData& frame, zebra::DescriptorLayoutCache& dcache, std::vector<zebra::render::RenderObject>& object_vector, zebra::render::Assets& assets, VkDescriptorSet& scene_set, bool bStatic) {
			const u64 BATCH_SIZE = 512;
			const u64 DRAW_OFFSET = BATCH_SIZE * sizeof(GPUObjectData);
			for (auto ridx = 0u;; ridx += BATCH_SIZE) {
				auto object_buffer = pop_buffer(renderer);
				MappedBuffer<GPUObjectData> object_map{ up.allocator, object_buffer };
				VkDrawIndirectCommand* draw_ptr = (VkDrawIndirectCommand*)(((u8*)object_map.data) + DRAW_OFFSET);

				VkDescriptorBufferInfo oinfo = {
					.buffer = object_buffer.buffer,
					.offset = 0,
					.range = DRAW_OFFSET,
				};

				VkDescriptorSet object_set;
				DescriptorBuilder::begin(up.device, frame.descriptor_pool, dcache)
					.bind_buffer(0, oinfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
					.build(object_set);


				auto left = std::min(BATCH_SIZE, object_vector.size() - ridx);
				auto batch_id = 0u;
				for (auto draw_i = 0; batch_id < left; draw_i += 1) {
					auto& prototype = object_vector[ridx + batch_id];
					auto& material = assets.t_materials[prototype.material_fk];
					auto& mesh = assets.t_meshes[prototype.mesh_fk];

					VkDrawIndirectCommand draw_command = {
						.vertexCount = (u32)assets.t_meshes[object_vector[ridx + batch_id].mesh_fk]._vertices.size(),
						.instanceCount = 0,
						.firstVertex = 0,
						.firstInstance = batch_id,
					};

					do {
						draw_command.instanceCount += 1;
						object_map[batch_id] = object_vector[ridx + batch_id].obj;
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
					vkCmdBindVertexBuffers(frame.buf, 0, 1, &mesh._vertex_buffer.buffer, &vertex_offset);
					vkCmdDrawIndirect(frame.buf, object_buffer.buffer, indirect_offset, 1, sizeof(VkDrawIndirectCommand));
				}



				if (left < BATCH_SIZE) {
					break;
				}
			}
		}
	}
}