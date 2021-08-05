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

#include "zebratypes.h"
#include "g_types.h"

#define DBG(x) std::cerr<<"["<<__func__<<"]"<<x<<std::endl;

#define VK_CHECK(x) do {\
VkResult err = x;\
if (err) { std::cerr << "[" << __func__ << "]" << "vulkan error: " << err << std::endl; abort();}\
} while (false)

namespace zebra { 
	
	struct VulkanSync {
		VkSemaphore presentS, renderS;
		VkFence renderF;
	};
	
	struct VulkanNative {
		vkb::Instance vkb_instance;
		vkb::Device vkb_device;
		VkInstance& instance() {
			return vkb_instance.instance;
		}
		VkDevice& device() {
			return vkb_device.device;
		}
		VkQueue graphics_queue;
		VkCommandPool command_pool;

		VkCommandBuffer cmd_main;
		VkRenderPass renderpass;
		VmaAllocator allocator;

		std::vector<VkFramebuffer> framebuffers;
		std::vector<VkImageView> image_views;
		std::vector<VkImage> images;
	};

	struct Window {
		GLFWwindow* handle;
		VkSurfaceKHR surface;
		vkb::Swapchain vkb_swapchain;
		VkSwapchainKHR& swapchain() {
			return vkb_swapchain.swapchain;
		}

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
		KeyModifier modifiers{ NONE };
		InputAction action{ NO_ACTION };
		bool match(Key k, KeyModifier m, KeyCondition c) {
			return key == k && (modifiers & m) == modifiers && condition == c;
		}
	};



	class zCore {
		VulkanNative _vk;
		VulkanSync _sync;
		Window _window;
		DeletionQueue main_delete_queue;

		bool die = false;

	protected:

		// Input

		// prologue to set state
		static void _glfw_key_callback_caller(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods);
		void _glfw_key_callback(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods);
		
		// epilogue, triggered by key event
		// pressed, release events are processed immediately
		// holds are deferred to the next tick
		void process_key_inputs();
		std::map<InputAction, std::function<void()>> action_map;
		std::vector<KeyInput> key_inputs;

		// Gfx
		bool init_vulkan();
		bool init_gfx();
		bool init_default_renderpass();
		bool init_commands();
		bool init_swapchain();
		bool init_framebuffers();
		bool init_sync();
		bool init_pipelines();
		bool recreate_swapchain();
		bool create_window();
		
		void app_loop();
		void draw_loop(std::stop_token stoken);
		void load_meshes();
		void upload_mesh(Mesh& mesh);

		// helpers
		bool load_shader_module(const char* file_path, VkShaderModule* out_shader);

	public:
		VkPipelineLayout _triangle_pipeline_layout;
		VkPipeline _triangle_pipeline;
		VkPipeline _colored_triangle_pipeline;
		VkPipeline _meshPipeline;
		Mesh _triangleMesh;
		u32 _selectedShader = 0u;


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
