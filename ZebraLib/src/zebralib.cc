#include "zebralib.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <vk_mem_alloc.h>
#include <thread>
#include <chrono>
#include <VkBootstrap.h>
#include <magic_enum.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "vki.h"
#include "g_types.h"
#include "g_pipeline.h"
#include "g_mesh.h"
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/scalar_constants.hpp>

namespace zebra {

	zCore::zCore() = default;
	zCore::~zCore() {
		this->cleanup();
	}

	bool zCore::start_application() {
		if (!init_gfx()) return false;
		init_scene();

		glfwSetWindowUserPointer(_window.handle, this);
		glfwSetKeyCallback(_window.handle, zCore::_glfw_key_callback_caller);

		this->action_map[EXIT_PROGRAM] = [this]() {
			this->die = true;
		};


		KeyInput exit;
		exit.key = Key{ GLFW_KEY_ESCAPE };
		exit.action = InputAction::EXIT_PROGRAM;
		this->key_inputs.push_back(exit);

		KeyInput next_shader;
		next_shader.key = Key{ GLFW_KEY_N };
		next_shader.action = InputAction::NEXT_SHADER;
		this->key_inputs.push_back(next_shader);

		app_loop();
		return true;
	}

	bool zCore::init_gfx() {
		DBG("start");

		if (glfwInit()) {
			DBG("glfw success");
		}

		DBG("window");
		if (!this->create_window()) return false;

		DBG("vulkan");
		if (!this->init_vulkan()) return false;

		DBG("swapchain");
		if (!this->init_swapchain()) return false;

		DBG("per frame data");
		if (!this->init_per_frame_data()) return false;

		DBG("render pass");
		if (!this->init_default_renderpass()) return false;

		DBG("framebuffers");
		if (!this->init_framebuffers()) return false;


		DBG("pipelines/shaders");
		if (!this->init_pipelines()) return false;

		DBG("-- resources");
		DBG("meshes");
		load_meshes();

		return true;
	}

	bool zCore::init_per_frame_data() {
		VkFenceCreateInfo fence_info = {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.pNext = nullptr,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT,
		};


		VkSemaphoreCreateInfo semaphore_info = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
		};

		auto poolinfo = vki::command_pool_create_info(
			_vk.vkb_device.get_queue_index(vkb::QueueType::graphics).value(),
			VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

		for (auto i = 0u; i < frames.size(); i++) {
			VK_CHECK(vkCreateCommandPool(_vk.device(), &poolinfo, nullptr, &frames[i].pool));
			auto cmd_info = vki::command_buffer_allocate_info(
				frames[i].pool);

			VK_CHECK(vkAllocateCommandBuffers(_vk.device(), &cmd_info, &frames[i].buf));
			VK_CHECK(vkCreateFence(_vk.device(), &fence_info, nullptr, &frames[i].renderF));
			VK_CHECK(vkCreateSemaphore(_vk.device(), &semaphore_info, nullptr, &frames[i].presentS));
			VK_CHECK(vkCreateSemaphore(_vk.device(), &semaphore_info, nullptr, &frames[i].renderS));
			swapchain_delq.push_function([=]() {
				vkDestroyCommandPool(_vk.device(), frames[i].pool, nullptr);
				vkDestroyFence(_vk.device(), frames[i].renderF, nullptr);
				vkDestroySemaphore(_vk.device(), frames[i].renderS, nullptr);
				vkDestroySemaphore(_vk.device(), frames[i].presentS, nullptr);
				});

		}
		return true;
	}

	bool zCore::init_default_renderpass() {
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

		VkAttachmentDescription depth_attachment = {
			.flags = 0,
			.format = _vk.depth_format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		};

		VkAttachmentReference depth_attachment_ref = {
			.attachment = 1,
			.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		};

		VkSubpassDescription subpass = {
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = 1,
			.pColorAttachments = &color_attachment_ref,
			.pDepthStencilAttachment = &depth_attachment_ref,
		};

		auto attachments = { color_attachment, depth_attachment };
		VkRenderPassCreateInfo render_pass_info = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = (u32)attachments.size(),
			.pAttachments = attachments.begin(),
			.subpassCount = 1,
			.pSubpasses = &subpass,
		};

		VK_CHECK(vkCreateRenderPass(_vk.device(), &render_pass_info, nullptr, &_vk.renderpass));
		swapchain_delq.push_function([=]() {
			vkDestroyRenderPass(_vk.device(), _vk.renderpass, nullptr);
			});
		return true;
	}

	bool zCore::init_framebuffers() {

		i32 w, h;
		w = _window.extent().width;
		h = _window.extent().height;

		VkFramebufferCreateInfo fb_info = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.renderPass = _vk.renderpass,
			.attachmentCount = 1,
			.width = (u32)w,
			.height = (u32)h,
			.layers = 1,
		};

		const u32 swapchain_imagecount = (u32)_vk.image_views.size();
		auto swapchain_imageviews = _vk.image_views;
		_vk.framebuffers = std::vector<VkFramebuffer>(_vk.image_views.size());

		for (auto i = 0u; i < swapchain_imagecount; i++) {
			auto attachments = { swapchain_imageviews[i], _vk.depth_image_view };
			fb_info.attachmentCount = attachments.size();
			fb_info.pAttachments = attachments.begin();
			VK_CHECK(vkCreateFramebuffer(_vk.device(), &fb_info, nullptr, &_vk.framebuffers[i]));
			swapchain_delq.push_function([=]() {
				vkDestroyFramebuffer(_vk.device(), _vk.framebuffers[i], nullptr);
				});
		}


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

	bool zCore::init_swapchain() {
		vkb::SwapchainBuilder swapchain_builder{ _vk.vkb_device };
		auto swap_ret = swapchain_builder
			.use_default_present_mode_selection()
			.build();
		if (!swap_ret) {
			DBG("creation failed. " << swap_ret.error().message());
			return false;
		}

		_window.vkb_swapchain = swap_ret.value(); 
		_vk.images = _window.vkb_swapchain.get_images().value();
		_vk.image_views = _window.vkb_swapchain.get_image_views().value();
		swapchain_delq.push_function([=]() {
			for (auto i = 0u; i < _vk.image_views.size(); i++) {
				vkDestroyImageView(_vk.device(), _vk.image_views[i], nullptr);
			}
			vkb::destroy_swapchain(_window.vkb_swapchain);
			});

		VkExtent3D depth_image_extent = {
			.width = _window.extent().width,
			.height = _window.extent().height,
			.depth = 1,
		};

		_vk.depth_format = VK_FORMAT_D32_SFLOAT;
		VkImageCreateInfo depth_image_info = vki::image_create_info(_vk.depth_format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depth_image_extent);
		VmaAllocationCreateInfo depth_image_allocinfo = {
			.usage = VMA_MEMORY_USAGE_GPU_ONLY,
			.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
		};

		vmaCreateImage(_vk.allocator, &depth_image_info, &depth_image_allocinfo, &_vk.depth_image.image, &_vk.depth_image.allocation, nullptr);
		VkImageViewCreateInfo depth_view_info = vki::imageview_create_info(_vk.depth_format, _vk.depth_image.image, VK_IMAGE_ASPECT_DEPTH_BIT);
		VK_CHECK(vkCreateImageView(_vk.device(), &depth_view_info, nullptr, &_vk.depth_image_view));

		swapchain_delq.push_function([=]() {
			vkDestroyImageView(_vk.device(), _vk.depth_image_view, nullptr);
			vmaDestroyImage(_vk.allocator, _vk.depth_image.image, _vk.depth_image.allocation);
		});

		return true;
	}

	bool zCore::recreate_swapchain() {

		vkDeviceWaitIdle(_vk.device());
		swapchain_delq.flush();
		DBG("recreate swapchain");
		if (!this->init_swapchain()) return false;

		DBG("per frame data");
		if (!this->init_per_frame_data()) return false;

		DBG("render pass");
		if (!this->init_default_renderpass()) return false;

		DBG("framebuffers");
		if (!this->init_framebuffers()) return false;
		return true;
	}

	bool zCore::init_vulkan() {
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

		VmaAllocatorCreateInfo allocate_info = {
			.physicalDevice = _vk.vkb_device.physical_device.physical_device,
			.device = _vk.device(),
			.instance = _vk.instance(),
		};
		vmaCreateAllocator(&allocate_info, &_vk.allocator);
		main_delq.push_function([=]() {
			vmaDestroyAllocator(_vk.allocator);
		});

		DBG("complete and ready to use.");
		return true;
	}

	bool zCore::init_pipelines() {

		VkShaderModule colored_triangle_frag;
		if (!load_shader_module("../shaders/colored_triangle.frag.spv", &colored_triangle_frag)) {
			DBG("error with colored triangle fragment shader");
		} else {
			DBG("colored triangle fragment shader loaded");
		}
		VkShaderModule mesh_triangle_vertex;
		if (!load_shader_module("../shaders/tri_mesh.vert.spv", &mesh_triangle_vertex)) {
			DBG("error with mesh vertex shader");
		} else {
			DBG("mesh vertex shader loaded");
		}


		VkPipelineLayoutCreateInfo pipeline_layout_info = vki::pipeline_layout_create_info();
		PipelineBuilder pipeline_builder;

		pipeline_builder._vertex_input_info = vki::vertex_input_state_create_info();
		pipeline_builder._input_assembly = vki::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
		pipeline_builder._viewport = {
			.x = 0.0f,
			.y = 0.0f,
			.width = (float)_window.vkb_swapchain.extent.width,
			.height = (float)_window.vkb_swapchain.extent.height,
			.minDepth = 0.0f,
			.maxDepth = 1.0f,
		};

		pipeline_builder._scissor = {
			.offset = { 0, 0 },
			.extent = _window.vkb_swapchain.extent,
		};

		pipeline_builder._rasterizer = vki::rasterization_state_create_info(VK_POLYGON_MODE_FILL);
		pipeline_builder._multisampling = vki::multisampling_state_create_info();
		pipeline_builder._color_blend_attachment = vki::color_blend_attachment_state();
		pipeline_builder._depth_stencil = vki::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

		// mesh
		pipeline_builder._shader_stages.clear();
		auto mesh_pipeline_layout_info = vki::pipeline_layout_create_info();
		VkPushConstantRange push_constant = {
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.offset = 0,
			.size = sizeof(MeshPushConstants),
		};
		mesh_pipeline_layout_info.pushConstantRangeCount = 1;
		mesh_pipeline_layout_info.pPushConstantRanges = &push_constant;
		VK_CHECK(vkCreatePipelineLayout(_vk.device(), &mesh_pipeline_layout_info, nullptr, &_mesh_pipeline_layout));

		pipeline_builder._shader_stages.push_back(
			vki::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, mesh_triangle_vertex)
		);
		pipeline_builder._shader_stages.push_back(
			vki::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, colored_triangle_frag)
		);

		auto vertex_description = P3N3C3::get_vertex_description();

		pipeline_builder._vertex_input_info.vertexAttributeDescriptionCount = vertex_description.attributes.size();
		pipeline_builder._vertex_input_info.pVertexAttributeDescriptions = vertex_description.attributes.data();
		pipeline_builder._vertex_input_info.vertexBindingDescriptionCount = vertex_description.bindings.size();
		pipeline_builder._vertex_input_info.pVertexBindingDescriptions = vertex_description.bindings.data();
		pipeline_builder._pipelineLayout = _mesh_pipeline_layout;

		_mesh_pipeline = pipeline_builder.build_pipeline(_vk.device(), _vk.renderpass);
		create_material(_mesh_pipeline, _mesh_pipeline_layout, "defaultmesh");

		//cleanup
		vkDestroyShaderModule(_vk.device(), colored_triangle_frag, nullptr);
		vkDestroyShaderModule(_vk.device(), mesh_triangle_vertex, nullptr);
		main_delq.push_function([=]() {
			vkDestroyPipelineLayout(_vk.device(), _mesh_pipeline_layout, nullptr);
			vkDestroyPipeline(_vk.device(), _mesh_pipeline, nullptr);
			});


		return true;
	}

	void zCore::init_scene() {
		RenderObject monkey;
		monkey.mesh = get_mesh("monkey");
		monkey.material = get_material("defaultmesh");
		monkey.transform = glm::mat4{ 1.0f };

		_renderables.push_back(monkey);
		for (int x = -20; x <= 20; x++) {
			for (int y = -20; y <= 20; y++) {

				RenderObject tri;
				tri.mesh = get_mesh("triangle");
				tri.material = get_material("defaultmesh");
				glm::mat4 translation = glm::translate(glm::mat4{ 1.0 }, glm::vec3(x, 0, y));
				glm::mat4 scale = glm::scale(glm::mat4{ 1.0 }, glm::vec3(0.2, 0.2, 0.2));
				tri.transform = translation * scale;

				_renderables.push_back(tri);
			}
		}
	}

	bool zCore::cleanup() {
		vkQueueWaitIdle(_vk.graphics_queue);
		swapchain_delq.flush();
		main_delq.flush();

		vkb::destroy_surface(_vk.vkb_instance, _window.surface);
		vkb::destroy_device(_vk.vkb_device);
		vkb::destroy_instance(_vk.vkb_instance);

		glfwDestroyWindow(_window.handle);
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
		std::filesystem::path file_path_std{ file_path };

		std::ifstream file(file_path, std::ios::ate | std::ios::binary);
		if (!file.is_open()) {
			DBG("couldnt open file");
			return false;
		}

		std::error_code pos_error;
		auto filesize = std::filesystem::file_size(file_path_std, pos_error);
		std::vector<uint32_t> buffer(filesize / sizeof(u32));

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

	bool zCore::advance_frame() {
		frame_counter = frame_counter + 1;
		return true;
	} 

	PerFrameData& zCore::current_frame() {
		return frames[frame_counter % FRAME_OVERLAP];
	}

	void zCore::load_meshes() {
		_triangle_mesh._vertices.resize(3);
		//vertex positions
		_triangle_mesh._vertices[0].pos = { 1.f, 1.f, 0.0f };
		_triangle_mesh._vertices[1].pos = {-1.f, 1.f, 0.0f };
		_triangle_mesh._vertices[2].pos = { 0.f,-1.f, 0.0f };

		//vertex colors, all green
		_triangle_mesh._vertices[0].color = { 0.f, 1.f, 0.0f }; //pure green
		_triangle_mesh._vertices[1].color = { 0.f, 1.f, 0.0f }; //pure green
		_triangle_mesh._vertices[2].color = { 0.f, 1.f, 0.0f }; //pure green

		upload_mesh(_triangle_mesh);

		_monkey_mesh.load_from_obj("../assets/monkey_smooth.obj");
		upload_mesh(_monkey_mesh);

		_meshes["monkey"] = _monkey_mesh;
		_meshes["triangle"] = _triangle_mesh;
	}

	void zCore::upload_mesh(Mesh& mesh) {
		VkBufferCreateInfo buffer_info = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = mesh._vertices.size() * sizeof(mesh._vertices[0]),
			.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		};

		VmaAllocationCreateInfo vma_alloc_info = {
			.usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
		};

		VK_CHECK(vmaCreateBuffer(_vk.allocator, &buffer_info, &vma_alloc_info, &mesh._vertex_buffer.buffer, &mesh._vertex_buffer.allocation, nullptr));

		main_delq.push_function([=]() {
			vmaDestroyBuffer(_vk.allocator, mesh._vertex_buffer.buffer, mesh._vertex_buffer.allocation);
			});

		void* data;
		vmaMapMemory(_vk.allocator, mesh._vertex_buffer.allocation, &data);
		memcpy(data, mesh._vertices.data(), mesh._vertices.size() * sizeof(mesh._vertices[0]));
		vmaUnmapMemory(_vk.allocator, mesh._vertex_buffer.allocation);
	}

	Material* zCore::create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name) {
		Material mat{
			.pipeline = pipeline,
			.pipeline_layout = layout,
		};
		_materials[name] = mat;
		return &_materials[name];
	}

	Material* zCore::get_material(const std::string& name) {
		auto it = _materials.find(name);
		if (it == _materials.end()) return nullptr;
		else return &(*it).second;
	}

	Mesh* zCore::get_mesh(const std::string& name) {
		auto it = _meshes.find(name);
		if (it == _meshes.end()) return nullptr;
		else return &(*it).second;
	}


	void zCore::draw_loop(std::stop_token stoken) {
		auto old_frame_start = std::chrono::steady_clock::now();

		auto monitor = glfwGetPrimaryMonitor();
		auto refresh_rate = glfwGetVideoMode(monitor)->refreshRate;
		auto dt = 1.f / 100.f;
		double global_current_time = 0.0;

		while (true) {
			auto start_frame = std::chrono::high_resolution_clock::now();
			auto time_passed = start_frame - old_frame_start;
			auto time_passed_float = std::chrono::duration<float>(time_passed).count();
			global_current_time += time_passed_float;

			if(vkGetFenceStatus(_vk.device(), current_frame().renderF) == VK_SUCCESS)
			{ // actual rendering
				vkResetFences(_vk.device(), 1, &current_frame().renderF);
				uint32_t swapchain_image_idx;
				auto acquire_result = vkAcquireNextImageKHR(_vk.device(), _window.swapchain(), 0, current_frame().presentS, nullptr, &swapchain_image_idx);

				if (acquire_result == VK_SUCCESS) {
					// we have an image to render to
					VK_CHECK(vkResetCommandBuffer(current_frame().buf, 0));
					VkCommandBufferBeginInfo cmd_begin_info = {
						.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
						.pNext = nullptr,
						.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
						.pInheritanceInfo = nullptr,
					};

					VK_CHECK(vkBeginCommandBuffer(current_frame().buf, &cmd_begin_info));
					VkClearValue clear_value;
					float flash = abs(sin(global_current_time));
					clear_value.color = { {0.f, 0.f, flash, 1.f} };

					VkClearValue depth_clear;
					depth_clear.depthStencil.depth = 1.f;

					auto clear_values = { clear_value, depth_clear };

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
					.clearValueCount = (u32)clear_values.size(),
					.pClearValues = clear_values.begin(),
					};

					vkCmdBeginRenderPass(current_frame().buf, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

					// -- dynamic state

					VkViewport viewport = {};
					viewport.height = _window.extent().height;
					viewport.width = _window.extent().width;
					viewport.minDepth = 0.0f;
					viewport.maxDepth = 1.0f;
					viewport.x = 0;
					viewport.y = 0;

					vkCmdSetViewport(current_frame().buf, 0, 1, &viewport);

					VkRect2D scissor = {
						.offset = { 0, 0 },
						.extent = _window.extent(),
					};

					vkCmdSetScissor(current_frame().buf, 0, 1, &scissor);

					// -- end dynamic state

					draw_objects(current_frame().buf, std::span<RenderObject>(_renderables.data(), _renderables.size()));

					vkCmdEndRenderPass(current_frame().buf);

					VK_CHECK(vkEndCommandBuffer(current_frame().buf));

					VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
					VkSubmitInfo submit_info = {
						.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
						.pNext = nullptr,
						.waitSemaphoreCount = 1,
						.pWaitSemaphores = &current_frame().presentS,
						.pWaitDstStageMask = &wait_stage,
						.commandBufferCount = 1,
						.pCommandBuffers = &current_frame().buf,
						.signalSemaphoreCount = 1,
						.pSignalSemaphores = &current_frame().renderS,
					};

					VK_CHECK(vkQueueSubmit(_vk.graphics_queue, 1, &submit_info, current_frame().renderF));
					VkPresentInfoKHR present_info = {
						.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
						.pNext = nullptr,
						.waitSemaphoreCount = 1,
						.pWaitSemaphores = &current_frame().renderS,
						.swapchainCount = 1,
						.pSwapchains = &_window.vkb_swapchain.swapchain,
						.pImageIndices = &swapchain_image_idx,
					};

					vkQueuePresentKHR(_vk.graphics_queue, &present_info);
					advance_frame();
				} else if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR || acquire_result == VK_SUBOPTIMAL_KHR) {
					this->recreate_swapchain();
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
	}

	void zCore::draw_objects(VkCommandBuffer cmd, std::span<RenderObject> render_objects) {
		glm::vec3 cam_pos = { 0.f, -6.f, -10.f };
		glm::mat4 view = glm::translate(glm::mat4(1.f), cam_pos);
		glm::mat4 projection = glm::perspective(glm::radians(70.f), 1700.f / 900.f, 0.1f, 200.0f);
		projection[1][1] *= -1; // for vulkan coordinates

		Mesh* last_mesh = nullptr;
		Material* last_material = nullptr;
		for (size_t i = 0; i < render_objects.size(); i++) {
			RenderObject& object = render_objects[i];
			if (object.material != last_material) {
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline);
				last_material = object.material;
			}

			glm::mat4 model = object.transform;
			glm::mat4 pvm = projection * view * model;

			MeshPushConstants constants;
			constants.render_matrix = pvm;
			vkCmdPushConstants(cmd, object.material->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(constants), &constants);

			if (object.mesh != last_mesh) {
				VkDeviceSize offset = 0;
				vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->_vertex_buffer.buffer, &offset);
				last_mesh = object.mesh;
			}

			vkCmdDraw(cmd, object.mesh->_vertices.size(), 1, 0, 0);
		}

	}

	// APP
	void zCore::app_loop() {

		std::jthread draw_thread([this](std::stop_token stoken){ 
			this->draw_loop(stoken);
		});


		while (!glfwWindowShouldClose(_window.handle)) {
			if (die) {
				DBG("time to die");
				return;
			}

			glfwPollEvents();
			std::this_thread::yield();
		}

		draw_thread.request_stop();
		draw_thread.join();
		this->cleanup();
	}
}