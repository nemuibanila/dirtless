#include "zebralib.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <vk_mem_alloc.h>
#include <thread>
#include <chrono>
#include <VkBootstrap.h>
#include <magic_enum.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vki.h"

namespace zebra {

	zCore::zCore() = default;
	zCore::~zCore() {
		this->cleanup();
	}

	bool zCore::start_application() {
		if (!initialize_gfx()) return false;

		glfwSetWindowUserPointer(_window.handle, this);
		glfwSetKeyCallback(_window.handle, zCore::_glfw_key_callback_caller);

		this->action_map[EXIT_PROGRAM] = [this]() {
			this->die = true;
		};

		KeyInput exit;
		exit.key = Key{ GLFW_KEY_ESCAPE };
		exit.action = InputAction::EXIT_PROGRAM;
		this->key_inputs.push_back(exit);

		app_loop();
		return true;
	}

	bool zCore::initialize_gfx() {
		DBG("start");

		if (glfwInit()) {
			DBG("glfw success");
		}

		

		DBG("window");
		if (!this->create_window()) return false;

		DBG("vulkan");
		if (!this->initialize_vulkan()) return false;

		DBG("swapchain");
		if (!this->initialize_swapchain()) return false;

		DBG("command pool");
		if (!this->initialize_commands()) return false;

		DBG("render pass");
		if (!this->initialize_default_renderpass()) return false;

		DBG("framebuffers");
		if (!this->initialize_framebuffers()) return false;

		DBG("sync");
		if (!this->initialize_sync()) return false;

		return true;
	}

	bool zCore::initialize_commands() {
		auto poolinfo = vki::command_pool_create_info(
			_vk.vkb_device.get_queue_index(vkb::QueueType::graphics).value(),
			VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

		VK_CHECK(vkCreateCommandPool(_vk.device(), &poolinfo, nullptr, &_vk.command_pool));
	
		auto cmd_info = vki::command_buffer_allocate_info(
			_vk.command_pool);

		VK_CHECK(vkAllocateCommandBuffers(_vk.device(), &cmd_info, &_vk.cmd_main));
		
		return true;
	}

	bool zCore::initialize_default_renderpass() {
		VkAttachmentDescription color_attachment = {
			.format = _window.vkb_swapchain.image_format,
			.samples = VK_SAMPLE_COUNT_1_BIT, // relevant for msaa
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		};

		VkAttachmentReference color_attachment_ref = {
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};

		VkSubpassDescription subpass = {
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = 1,
			.pColorAttachments = &color_attachment_ref,
		};

		VkRenderPassCreateInfo render_pass_info = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = 1,
			.pAttachments = &color_attachment,
			.subpassCount = 1,
			.pSubpasses = &subpass,
		};

		VK_CHECK(vkCreateRenderPass(_vk.device(), &render_pass_info, nullptr, &_vk.renderpass));
		return true;
	}

	bool zCore::initialize_framebuffers() {

		i32 w, h;
		glfwGetFramebufferSize(_window.handle, &w, &h);

		VkFramebufferCreateInfo fb_info = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.renderPass = _vk.renderpass,
			.attachmentCount = 1,
			.width = (u32)w,
			.height = (u32)h,
			.layers = 1,
		};

		const u32 swapchain_imagecount = _window.vkb_swapchain.image_count;
		auto swapchain_imageviews = _window.vkb_swapchain.get_image_views().value();
		_vk.framebuffers = std::vector<VkFramebuffer>(swapchain_imagecount);

		for (auto i = 0; i < swapchain_imagecount; i++) {
			fb_info.pAttachments = &swapchain_imageviews[i];
			VK_CHECK(vkCreateFramebuffer(_vk.device(), &fb_info, nullptr, &_vk.framebuffers[i]));
		}

	}

	bool zCore::initialize_sync() {
		VkFenceCreateInfo fence_info = {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.pNext = nullptr,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT,
		};

		VK_CHECK(vkCreateFence(_vk.device(), &fence_info, nullptr, &_sync.renderF));

		VkSemaphoreCreateInfo semaphore_info = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
		};

		VK_CHECK(vkCreateSemaphore(_vk.device(), &semaphore_info, nullptr, &_sync.presentS));
		VK_CHECK(vkCreateSemaphore(_vk.device(), &semaphore_info, nullptr, &_sync.renderS));
		return true;
	}

	bool zCore::create_window() {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		_window.handle = glfwCreateWindow(1280, 720, "Zebra", NULL, NULL);
		if (_window.handle == nullptr) {
			DBG("Failed: nullptr");
			return false;
		}


		return true;
	}

	bool zCore::initialize_swapchain() {
		vkb::SwapchainBuilder swapchain_builder{ _vk.vkb_device };
		auto swap_ret = swapchain_builder.build();
		if (!swap_ret) {
			DBG("creation failed. " << swap_ret.error().message());
			return false;
		}

		_window.vkb_swapchain = swap_ret.value(); 
		return true;
	}

	bool zCore::recreate_swapchain() {
		vkb::SwapchainBuilder swapchain_builder{ _vk.vkb_device };
		auto swap_ret = swapchain_builder
			.set_old_swapchain(_window.vkb_swapchain)
			.build();
		if (!swap_ret) {
			_window.vkb_swapchain.swapchain = VK_NULL_HANDLE;
		}
		
		vkb::destroy_swapchain(_window.vkb_swapchain);
		_window.vkb_swapchain = swap_ret.value(); 
		return true;
	}

	bool zCore::initialize_vulkan() {
		if (!glfwVulkanSupported()) {
			DBG("Vulkan is not supported.");
			return false;
		}

		u32 ext_count;
		auto extensions = glfwGetRequiredInstanceExtensions(&ext_count);

		// adapted from https://github.com/charles-lunarg/vk-bootstrap example
		// under MIT license

		vkb::InstanceBuilder builder;
		builder
			.set_app_name("Zebra")
			.request_validation_layers()
			.use_default_debug_messenger();

		for (u32 i = 0; i < ext_count; i++) {
			builder.enable_extension(extensions[i]);
		}
			
		auto inst_ret =	builder.build();
		if (!inst_ret) {
			DBG("instance err: " << inst_ret.error().message());
			return false;
		}
		_vk.vkb_instance = inst_ret.value();

		glfwCreateWindowSurface(_vk.instance(), _window.handle, nullptr, &_window.surface);

		vkb::PhysicalDeviceSelector selector{ _vk.vkb_instance };
		auto phys_ret = selector
			.set_surface(_window.surface)
			.set_minimum_version(1, 2)
			.prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
			.select();
		if (!phys_ret) {
			DBG("physical device selection err: " << phys_ret.error().message());
			return false;
		}

		vkb::DeviceBuilder device_builder{ phys_ret.value() };
		auto dev_ret = device_builder.build();
		if (!dev_ret) {
			DBG("device err: " << dev_ret.error().message());
			return false;
		}
		_vk.vkb_device = dev_ret.value();

		auto graphics_queue_ret = _vk.vkb_device.get_queue(vkb::QueueType::graphics);
		if (!graphics_queue_ret) {
			DBG("no graphics queue: " << graphics_queue_ret.error().message());
			return false;
		}
		_vk.graphics_queue = graphics_queue_ret.value();
		DBG("complete and ready to use.");

		return true;
	}

	bool zCore::cleanup() {

		vkDestroyCommandPool(_vk.device(), _vk.command_pool, nullptr);
		
		
		vkb::destroy_swapchain(_window.vkb_swapchain);
		vkDestroyRenderPass(_vk.device(), _vk.renderpass, nullptr);
		
		for (auto i = 0u; i < _vk.framebuffers.size(); i++) {
			vkDestroyFramebuffer(_vk.device(), _vk.framebuffers[i], nullptr);
		}

		vkb::destroy_device(_vk.vkb_device);
		vkb::destroy_surface(_vk.vkb_instance, _window.surface);
		vkb::destroy_instance(_vk.vkb_instance);

		glfwDestroyWindow(_window.handle);
		glfwTerminate();
		return true;
	}


	// INPUT

	void zCore::_glfw_key_callback_caller(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods) {
		zCore* zcore = (zCore*)glfwGetWindowUserPointer(window);
		zcore->_glfw_key_callback(window, key, scancode, action, mods);
	}

	void zCore::_glfw_key_callback(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods) {
		Key k{ key };
		KeyCondition cond;
		KeyModifier mod = static_cast<KeyModifier>(mods);
		if (action == GLFW_PRESS) {
			cond = KeyCondition::PRESSED;
		} else if (action == GLFW_RELEASE) {
			cond = KeyCondition::RELEASE;
		} else {
			return;
		}

		for (auto input : key_inputs) {
			if (input.match(k, mod, cond)) {
				auto fnc = action_map.find(input.action);
				if (fnc != action_map.end()) {
					fnc->second();
				} else {
					DBG("no input action function for " << magic_enum::enum_name(input.action) << " (ID: " << input.action << ")");
				}

			}
		}

	}

	bool zCore::is_key_held(const Key& k) {
		return glfwGetKey(_window.handle, k.keycode);
	}

	void zCore::process_key_inputs() {

	}

	bool zCore::load_shader_module(const char* file_path, VkShaderModule* out_shader) {
		std::ifstream file(file_path, std::ios::ate | std::ios::binary);
		if (!file.is_open()) return false;

		auto filesize = file.tellg();
		std::vector<uint32_t> buffer(filesize / sizeof(u32) + 1);

		file.seekg(0);
		file.read((char*)buffer.data(), filesize);
		file.close();

		VkShaderModuleCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.pNext = nullptr,
			.codeSize = buffer.size() * sizeof(u32),
			.pCode = buffer.data(),
		};

		VkShaderModule shader_module;
		if (vkCreateShaderModule(_vk.device(), &create_info, nullptr, &shader_module) != VK_SUCCESS) {
			return false;
		}

		*out_shader = shader_module;
		return true;
	}

	// APP
	void zCore::app_loop() {

		std::jthread draw_thread([this](std::stop_token stoken){ 
			auto old_frame_start = std::chrono::steady_clock::now();
			auto dt = 1.f / 60.f;
			auto time_acc = 0.f;
			double global_current_time = 0.0;

			while (true) {
				auto start_frame = std::chrono::high_resolution_clock::now();
				auto time_passed = start_frame - old_frame_start;
				auto time_passed_float = std::chrono::duration<float>(time_passed).count();
				global_current_time += time_passed_float;
				time_acc += time_passed_float;

				if (time_acc > dt) {
					// do time step
					DBG("time step! " << time_acc);
					time_acc -= dt;
					global_current_time += dt;
				}
				
				if(vkGetFenceStatus(_vk.device(), _sync.renderF) == VK_SUCCESS)
				{ // actual rendering
					vkResetFences(_vk.device(), 1, &_sync.renderF);
					uint32_t swapchain_image_idx;
					auto acquire_result = vkAcquireNextImageKHR(_vk.device(), _window.swapchain(), 0, _sync.presentS, nullptr, &swapchain_image_idx);

					if (acquire_result == VK_SUCCESS || acquire_result == VK_SUBOPTIMAL_KHR) {
						// we have an image to render to
						VK_CHECK(vkResetCommandBuffer(_vk.cmd_main, 0));
						VkCommandBufferBeginInfo cmd_begin_info = {
							.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
							.pNext = nullptr,
							.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
							.pInheritanceInfo = nullptr,
						};

						VK_CHECK(vkBeginCommandBuffer(_vk.cmd_main, &cmd_begin_info));
						VkClearValue clear_value;
						float flash = abs(sin(global_current_time));
						clear_value.color = { {0.f, 0.f, flash, 1.f} };

						VkRenderPassBeginInfo rp_info = {
							.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
							.pNext = nullptr,
							.renderPass = _vk.renderpass,
							.framebuffer = _vk.framebuffers[swapchain_image_idx],
							.renderArea = {
								.offset = {
									.x = 0,
									.y = 0,
								},
								.extent = _window.vkb_swapchain.extent,
							},
							.clearValueCount = 1,
							.pClearValues = &clear_value,
						};

						vkCmdBeginRenderPass(_vk.cmd_main, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
						vkCmdEndRenderPass(_vk.cmd_main);

						VK_CHECK(vkEndCommandBuffer(_vk.cmd_main));

						VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
						VkSubmitInfo submit_info = {
							.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
							.pNext = nullptr,
							.waitSemaphoreCount = 1,
							.pWaitSemaphores = &_sync.presentS,
							.pWaitDstStageMask = &wait_stage,
							.commandBufferCount = 1,
							.pCommandBuffers = &_vk.cmd_main,
							.signalSemaphoreCount = 1,
							.pSignalSemaphores = &_sync.renderS,
						};

						VK_CHECK(vkQueueSubmit(_vk.graphics_queue, 1, &submit_info, _sync.renderF));
						VkPresentInfoKHR present_info = {
							.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
							.pNext = nullptr,
							.waitSemaphoreCount = 1,
							.pWaitSemaphores = &_sync.renderS,
							.swapchainCount = 1,
							.pSwapchains = &_window.vkb_swapchain.swapchain,
							.pImageIndices = &swapchain_image_idx,
						};

						VK_CHECK(vkQueuePresentKHR(_vk.graphics_queue, &present_info));
					} 
				}

				if (stoken.stop_requested()) {
					DBG("stop requested");
					std::this_thread::sleep_for(std::chrono::seconds(1));
					return;
				}

				old_frame_start = start_frame;
				std::this_thread::yield();
			};
		});


		while (!glfwWindowShouldClose(_window.handle)) {
			if (die) {
				DBG("time to die");
				return;
			}

			glfwPollEvents();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		draw_thread.request_stop();
	}
}