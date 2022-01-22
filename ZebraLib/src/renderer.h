#pragma once
#include <map>
#include <vector>
#include <unordered_map>
#include "g_types.h"
#include "g_mesh.h"
#include "g_descriptorset.h"
#include "zebratypes.h"
#include <deque>
#include <span>
#include "boost/container_hash/hash.hpp"

namespace zebra {
	namespace render {
		union CullSphere {
			glm::vec4 sphere;
			struct {
				float center[3];
				float radius;
			};
		};

		struct RenderObject {
			GPUObjectData obj;
			CullSphere cull;
			u64 material_fk;
			u64 mesh_fk;
		};

		struct RenderData {
			VkRenderPass forward_pass;
			VkFramebuffer forward_framebuffer;
			VkExtent2D forward_extent;
			DescriptorLayoutCache* dcache;
		};

		struct CullInfo {

		};

		struct UsedBuffer {
			AllocBuffer buffer;
		};

		struct DangerBuffer {
			VkFence fence_usage;
			AllocBuffer buffer;
		};

		struct StaticDrawInfo {
			Material material;
			Mesh* mesh;
			VkDeviceSize indirect_offset;
		};

		struct Renderer {
			std::vector<RenderObject> t_objects;
			// refactor this into one big buffer uwu
			std::deque<AllocBuffer> available_buffers;
			std::deque<UsedBuffer> used_buffers;
			std::deque<DangerBuffer> danger_buffers;

			std::vector<RenderObject> t_statics;
			std::vector<AllocBuffer> static_buffers;
			std::vector<StaticDrawInfo> static_draws;

			UploadContext* up;
			bool b_statics_sorted = false;
		};

		struct Assets {
			std::unordered_map<std::string, u64> t_mat_index;
			std::unordered_map<u64, Material> t_materials;
			std::unordered_map<u64, Mesh> t_meshes;
			std::unordered_map<u64, Texture> t_textures;
			std::unordered_map<std::string, u64> t_names;
		};

		struct SceneParameters {
		};

		const u32 SINGLE_BUFFER_SIZE = 131072;
		u64 insert_mesh(Assets& assets, Mesh mesh);
		u64 insert_material(Assets& assets, Material material);
		u64 insert_texture(Assets& assets, Texture material);
		void name_handle(Assets& assets, std::string name, u64 handle);

		void begin_collect(Renderer& renderer, UploadContext& up);
		void add_renderable(Renderer& renderer, RenderObject object, bool bStatic = false);
		void finish_collect(Renderer& renderer);
		void render(Renderer& renderer, Assets& assets, PerFrameData& frame, UploadContext& up, GPUSceneData& params, RenderData& rdata);
		void draw_batches(zebra::render::Renderer& renderer, zebra::UploadContext& up, zebra::PerFrameData& frame, zebra::DescriptorLayoutCache& dcache, std::vector<zebra::render::RenderObject>& object_vector, zebra::render::Assets& assets, VkDescriptorSet& scene_set);
		void clear_buffers(zebra::render::Renderer& renderer, zebra::UploadContext& up);

		struct DependencyInfo {
			VkFormat format;
			VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
			VkImageLayout final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			VkAttachmentLoadOp on_load = VK_ATTACHMENT_LOAD_OP_CLEAR;
			VkAttachmentStoreOp on_store = VK_ATTACHMENT_STORE_OP_STORE;
			VkImageLayout attachment_layout = VK_IMAGE_LAYOUT_UNDEFINED;
		};
		

		struct DependencyInfoHash {
			std::size_t operator()(std::span<DependencyInfo> const& sdi) const noexcept {
				size_t seed = 0;
				for (auto& di : sdi) {
					boost::hash_combine(seed, di.format);
					boost::hash_combine(seed, di.attachment_layout);
					boost::hash_combine(seed, di.final_layout);
					boost::hash_combine(seed, di.on_load);
					boost::hash_combine(seed, di.on_store);
				}
				return seed;
			}
		};
		
		struct DependencyInfoEq {
			constexpr bool operator()(const std::span<DependencyInfo>& lhs, const std::span<DependencyInfo>& rhs) const {
				if (lhs.size() != rhs.size()) return false;
				else {
					return memcmp(lhs.data(), rhs.data(), lhs.size_bytes()) == 0;
				}
			}
		};
		
		struct RenderPassCache {
			VkDevice device;
			std::unordered_map<std::span<DependencyInfo>, VkRenderPass, DependencyInfoHash, DependencyInfoEq> cache;
			VkRenderPass get_or_create(std::span<DependencyInfo> info);
			void clear();
			
			
			RenderPassCache(VkDevice device = VK_NULL_HANDLE) {
				this->device = device;
			}
		};

		AllocBuffer pop_buffer(Renderer& renderer, bool bJustTake = false);
		AllocBuffer pop_hot_buffer(Renderer& renderer, VkFence render_fence);
	}
}
