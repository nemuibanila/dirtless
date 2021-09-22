#pragma once
#include <map>
#include <vector>
#include <unordered_map>
#include "g_types.h"
#include "g_mesh.h"
#include "zebratypes.h"
#include <deque>

namespace zebra {
	namespace render {
		union CullSphere {
			glm::vec4 sphere;
			struct {
				glm::vec3 center;
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

		struct Renderer {
			std::vector<RenderObject> t_objects;
			std::vector<RenderObject> sorted_objects;
			std::deque<AllocBuffer> available_buffers;
			std::deque<UsedBuffer> used_buffers;

			UploadContext up;
		};

		struct Assets {
			std::unordered_map<std::string, u64> t_mat_index;
			std::unordered_map<u64, Material> t_materials;
			std::unordered_map<u64, Mesh> t_meshes;
		};

		struct SceneParameters {
		};
		
		u64 insert_mesh(Assets& assets, Mesh mesh);
		u64 insert_material(Assets& assets, Material material);

		void begin_collect(Renderer& renderer);
		void add_renderable(Renderer& renderer, RenderObject object);
		void finish_collect(Renderer& renderer);
		void render(Renderer& renderer, Assets& assets, GPUSceneData& params);
		AllocBuffer pop_buffer(Renderer& renderer);
	}
}
