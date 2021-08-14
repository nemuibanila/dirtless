#pragma once
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>
#include <map>
#include <vector>
#include <functional>
#include <queue>
#include <set>
#include <thread>
#include <chrono>
#include <span>

#include "zebratypes.h"
#include "g_types.h"
#include "g_camera.h"
#include "g_mesh.h"
#include "z_debug.h"


namespace zebra { 

	struct PerFrameData {
		VkSemaphore presentS, renderS;
		VkFence renderF;

		VkCommandPool pool;
		VkCommandBuffer buf;

		AllocBuffer camera_buffer;
		VkDescriptorSet global_descriptor;
	};

	struct UploadContext {
		VkFence uploadF;
		VkCommandPool pool;
	};
	
	struct VulkanNative {

		VkInstance& instance() {
			return vkb_instance.instance;
		}
		VkDevice& device() {
			return vkb_device.device;
		}
		VkQueue graphics_queue;
		VkRenderPass renderpass;
		VmaAllocator allocator;

		std::vector<VkFramebuffer> framebuffers;
		std::vector<VkImageView> image_views;
		std::vector<VkImage> images;

		VkImageView depth_image_view;
		AllocImage depth_image;
		VkFormat depth_format;

		VkDescriptorSetLayout global_set_layout;
		VkDescriptorPool descriptor_pool;

		VkPhysicalDeviceProperties gpu_properties;

		vkb::Instance vkb_instance;
		vkb::Device vkb_device;
	};

	struct Window {
		GLFWwindow* handle;
		VkSurfaceKHR surface;
		vkb::Swapchain vkb_swapchain;
		VkSwapchainKHR& swapchain() {
			return vkb_swapchain.swapchain;
		}

		VkExtent2D extent() const {
			return vkb_swapchain.extent;
		};

	};

	struct DrawFrameInfo {
		std::chrono::steady_clock::time_point old_frame_start;
		double global_current_time;
	};

	struct DeletionQueue
	{
		std::deque<std::function<void()>> deletors;

		void push_function(std::function<void()>&& function) {
			deletors.push_back(function);
		}

		void flush() {
			// reverse iterate the deletion queue to execute all the functions
			for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
				(*it)(); //call the function
			}

			deletors.clear();
		}
	};

	enum InputAction {
#include "input_actions.strings"
		INPUT_ACTION_COUNT,
	};

	struct Key {
		i32 keycode;
		bool operator<(const Key& k) const {
			return keycode < k.keycode;
		}
		bool operator==(const Key& k) const {
			return keycode == k.keycode;
		}

	};

	/* pressed and release are exclusive,
	* while hold acts like a flag */
	enum KeyCondition {
		RELEASE = GLFW_RELEASE,
		PRESSED = GLFW_PRESS,
		HOLD = 2,
	};

	enum KeyModifier {
		NONE = 0,
		SHIFT = 1,
		CONTROL = 2,
		ALT = 4,
		SUPER = 8,
		CAPS = 16,
		NUM = 32,
	};

	struct KeyInput {
		Key key{ 0 };
		KeyCondition condition = KeyCondition::PRESSED;
		/// @brief Hold keys can not use modifiers
		KeyModifier modifiers{ NONE };
		InputAction action{ NO_ACTION };
		bool match(Key k, KeyModifier m, KeyCondition c) {
			return key == k && (modifiers & m) == modifiers && condition == c;
		}
	};


	constexpr u32 FRAME_OVERLAP = 2u;
	constexpr float TICK_DT = 1.f / 100.f;

	class zCore {
	protected:
		// -- cleanup
		DeletionQueue main_delq;
		DeletionQueue swapchain_delq;
		bool die = false;

		// -- rendering
		VulkanNative _vk;
		UploadContext _up;
		DrawFrameInfo _df;
		Window _window;
		std::array<PerFrameData, FRAME_OVERLAP> frames;
		size_t frame_counter = 0;




		// -- Input
		// 
		// prologue to set state
		static void _glfw_key_callback_caller(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods);
		void _glfw_key_callback(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods);
		
		glm::vec2 _mouse_old_pos;
		static void _glfw_mouse_position_callback_caller(GLFWwindow* window, double x, double y);
		void _glfw_mouse_position_callback(GLFWwindow* window, double x, double y);

		// epilogue, triggered by key event
		// pressed, release events are processed immediately
		// holds are deferred to the next tick
		void process_key_inputs();
		void process_mouse_inputs();
		void set_cursor_absolute(bool absolute);

		// Gfx
		bool init_vulkan();
		bool init_gfx();
		bool init_default_renderpass();
		bool init_per_frame_data();
		bool init_swapchain();
		bool init_framebuffers();
		bool init_pipelines();
		void init_descriptor_set_layouts();
		void init_descriptor_sets();
		void init_upload_context();
		void init_scene();
		void init_imgui();
		bool recreate_swapchain();
		bool create_window();
		
		void app_loop();
		void setup_draw();
		void draw();
		void draw_objects(VkCommandBuffer cmd, std::span<RenderObject> render_objects);
		void load_meshes();



	public:
		// -- rendering
		bool load_shader_module(const char* file_path, VkShaderModule* out_shader);
		void upload_mesh(Mesh& mesh);
		Material* create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);
		Material* get_material(const std::string& name);
		Mesh* get_mesh(const std::string& name);
		AllocBuffer create_buffer(size_t alloc_size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage);
		void vk_immediate(std::function<void(VkCommandBuffer cmd)>&& function);
		size_t pad_uniform_buffer_size(size_t original_size);

		bool advance_frame();
		PerFrameData& current_frame();
		u32 current_frame_idx();

		VkPipelineLayout _mesh_pipeline_layout;
		VkPipeline _mesh_pipeline;

		GPUSceneData scene_parameters;
		AllocBuffer scene_parameter_buffer;

		std::vector<RenderObject> _renderables;
		std::unordered_map<std::string, Material> _materials;
		std::unordered_map<std::string, Mesh> _meshes;

		FirstPersonPerspectiveCamera _camera;

		Mesh _triangle_mesh;
		Mesh _monkey_mesh;

		// -- input 
		std::map<InputAction, std::function<void()>> action_map;
		std::vector<KeyInput> key_inputs;
		bool cursor_use_absolute_position = false;
		bool invert_camera = false;
		glm::vec2 mouse_delta_sens = glm::vec2(0.01f, 0.01f);
		glm::vec2 mouse_delta = glm::vec2(0.f);

		zCore();
		virtual ~zCore();
		zCore(const zCore&) = delete;
		zCore& operator=(zCore const&) = delete;

		// initialization functions
		bool start_application();
		bool cleanup();

		// input functions
		bool is_key_held(const Key& k);

	};

}  // namespace zebra
