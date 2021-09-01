#include "zebralib.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <vk_mem_alloc.h>
#include <thread>
#include <chrono>
#include <random>
#include <VkBootstrap.h>
#include <magic_enum.h>
#include <numeric>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "vki.h"
#include "g_types.h"
#include "g_pipeline.h"
#include "g_mesh.h"
#include "g_vec.h"
#include "g_texture.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <boost/circular_buffer.hpp>

namespace zebra {

	zCore::zCore() = default;
	zCore::~zCore() {
		this->cleanup();
	}

	// OK 01.09.2021
	bool zCore::start_application() {
		if (!init_gfx()) return false;
		init_scene();

		glfwSetWindowUserPointer(_window.handle, this);
		glfwSetKeyCallback(_window.handle, zCore::_glfw_key_callback_caller);
		glfwSetCursorPosCallback(_window.handle, zCore::_glfw_mouse_position_callback_caller);

		double x, y;
		glfwGetCursorPos(_window.handle, &x, &y);
		_mouse_old_pos = { x, y };

		if (glfwRawMouseMotionSupported()) {
			glfwSetInputMode(_window.handle, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
		}

		// -- debugging camera and such
		this->action_map[EXIT_PROGRAM] = [this]() {
			this->die = true;
		};

		this->action_map[TOGGLE_ABSOLUTE_MOUSE] = [this]() {
			set_cursor_absolute(this->cursor_use_absolute_position ^ 1);
		};

		KeyInput toggle_absolute{
			.key = GLFW_KEY_K,
			.action = InputAction::TOGGLE_ABSOLUTE_MOUSE
		};
		this->key_inputs.push_back(toggle_absolute);

		
		KeyInput exit;
		exit.key = Key{ GLFW_KEY_ESCAPE };
		exit.action = InputAction::EXIT_PROGRAM;
		this->key_inputs.push_back(exit);

		KeyInput next_shader;
		next_shader.key = Key{ GLFW_KEY_N };
		next_shader.action = InputAction::NEXT_SHADER;
		this->key_inputs.push_back(next_shader);
		
		KeyInput forward;
		forward.key = Key{ GLFW_KEY_W };
		forward.condition = KeyCondition::HOLD;
		forward.action = InputAction::MOVE_FORWARD;
		this->key_inputs.push_back(forward);

		KeyInput back;
		back.key = Key{ GLFW_KEY_S };
		back.condition = KeyCondition::HOLD;
		back.action = InputAction::MOVE_BACK;
		this->key_inputs.push_back(back);
		
		KeyInput left;
		left.key = Key{ GLFW_KEY_A };
		left.condition = KeyCondition::HOLD;
		left.action = InputAction::MOVE_STRAFE_LEFT;
		this->key_inputs.push_back(left);

		KeyInput right;
		right.key = Key{ GLFW_KEY_D };
		right.condition = KeyCondition::HOLD;
		right.action = InputAction::MOVE_STRAFE_RIGHT;
		this->key_inputs.push_back(right);

		KeyInput up{
			.key = GLFW_KEY_SPACE,
			.condition = KeyCondition::HOLD,
			.action = InputAction::MOVE_FLY_UP,
		};
		key_inputs.push_back(up);

		KeyInput down{
			.key = GLFW_KEY_C,
			.condition = KeyCondition::HOLD,
			.action = InputAction::MOVE_FLY_DOWN,
		};
		key_inputs.push_back(down);
		// --

		setup_draw();

		// -- init imgui late, to overwrite callbacks
		init_imgui();
		set_cursor_absolute(false);

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
		if (!this->init_swapchain_per_frame_data()) return false; 
		init_upload_context();

		DBG("render pass");
		if (!this->init_default_renderpass()) return false; // ???

		DBG("framebuffers");
		if (!this->init_framebuffers()) return false;

		DBG("pipelines/shaders");
		init_descriptor_set_layouts();

		init_per_frame_data();
		if (!this->init_pipelines()) return false;


		DBG("-- resources");
		load_images();

		DBG("meshes");
		load_meshes();

		DBG("camera");
		auto rotation_initial = glm::quat(1.f, 0.f, 0.f, 0.f);
		_camera = {
			._pos = glm::vec3(0.f),
			.movement_smoothing = 4.f,
			.aspect = 16.f / 9.f,
			.povy = 70.f,
			.z_near = 0.01f,
			.z_far = 200.f,
		};
		return true;
	}

	// OBJECT SYSTEM IN NEED OF REWORK
	const int MAX_OBJECTS = 640000;
	bool zCore::init_per_frame_data() {
		for (auto i = 0u; i < FRAME_OVERLAP; i++) {

			// grow dynamically eventually

			frames[i].object_buffer = create_buffer(_vk.allocator, sizeof(GPUObjectData) * MAX_OBJECTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
			frames[i].makeup_buffer = create_buffer(_vk.allocator, sizeof(GPUMakeupData) * MAX_OBJECTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

			main_delq.push_function([=]() {
				vmaDestroyBuffer(_vk.allocator, frames[i].makeup_buffer.buffer, frames[i].makeup_buffer.allocation);
				vmaDestroyBuffer(_vk.allocator, frames[i].object_buffer.buffer, frames[i].object_buffer.allocation);
				});
		}



		init_descriptor_sets();
		return true;
	}

	bool zCore::init_swapchain_per_frame_data() {
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
			fb_info.attachmentCount = (u32)attachments.size();
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
		vkDeviceWaitIdle(_vk.device());

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
		int w, h;
		do {
			glfwGetWindowSize(this->_window.handle, &w, &h);
			if (w == 0 && h == 0) {
				DBG("window is currently 0-sized");
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				glfwPollEvents();
			}
		} while (w == 0 && h == 0);

		DBG("recreate swapchain");
		if (!this->init_swapchain()) return false;

		DBG("per frame data");
		if (!this->init_swapchain_per_frame_data()) return false;

		DBG("render pass");
		if (!this->init_default_renderpass()) return false;

		DBG("framebuffers");
		if (!this->init_framebuffers()) return false;
		return true;
	}

	// OK 01.09.2021
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
			//.request_validation_layers()
			.use_default_debug_messenger();

		for (u32 i = 0; i < ext_count; i++) {
			builder.enable_extension(extensions[i]);
		}

		auto inst_ret = builder.build();
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

		vkGetPhysicalDeviceProperties(_vk.vkb_device.physical_device.physical_device, &_vk.gpu_properties);
		DBG("gpu minimum buffer alignment: " << _vk.gpu_properties.limits.minUniformBufferOffsetAlignment);

		DBG("complete and ready to use.");
		return true;
	}

	bool zCore::init_pipelines() {

		VkShaderModule default_lit_frag;
		if (!load_shader_module("../shaders/default_lit.frag.spv", &default_lit_frag)) {
			DBG("error with default lit fragment shader");
		} else {
			DBG("default lit fragment shader loaded");
		}
		VkShaderModule mesh_triangle_vertex;
		if (!load_shader_module("../shaders/tri_mesh.vert.spv", &mesh_triangle_vertex)) {
			DBG("error with mesh vertex shader");
		} else {
			DBG("mesh vertex shader loaded");
		}
		VkShaderModule textured_mesh_shader;
		if (!load_shader_module("../shaders/textured_lit.frag.spv", &textured_mesh_shader)) {
			DBG("error with textured_lit fragment shader");
		} else {
			DBG("textured_lit fragment shader loaded");
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

		// mesh color shader
		pipeline_builder._shader_stages.clear();
		auto mesh_pipeline_layout_info = vki::pipeline_layout_create_info();
		VkPushConstantRange push_constant = {
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.offset = 0,
			.size = sizeof(MeshPushConstants),
		};
		mesh_pipeline_layout_info.pushConstantRangeCount = 1;
		mesh_pipeline_layout_info.pPushConstantRanges = &push_constant;

		auto set_layouts = { _vk.global_set_layout, _vk.object_set_layout };
		mesh_pipeline_layout_info.setLayoutCount = (u32)set_layouts.size();
		mesh_pipeline_layout_info.pSetLayouts = set_layouts.begin();

		VK_CHECK(vkCreatePipelineLayout(_vk.device(), &mesh_pipeline_layout_info, nullptr, &_mesh_pipeline_layout));

		pipeline_builder._shader_stages.push_back(
			vki::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, mesh_triangle_vertex)
		);
		pipeline_builder._shader_stages.push_back(
			vki::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, default_lit_frag)
		);


		auto vertex_description = P3N3C3U2::get_vertex_description();
		pipeline_builder._vertex_input_info.vertexAttributeDescriptionCount = (u32)vertex_description.attributes.size();
		pipeline_builder._vertex_input_info.pVertexAttributeDescriptions = vertex_description.attributes.data();
		pipeline_builder._vertex_input_info.vertexBindingDescriptionCount = (u32)vertex_description.bindings.size();
		pipeline_builder._vertex_input_info.pVertexBindingDescriptions = vertex_description.bindings.data();
		pipeline_builder._pipelineLayout = _mesh_pipeline_layout;

		_mesh_pipeline = pipeline_builder.build_pipeline(_vk.device(), _vk.renderpass);
		create_material(_mesh_pipeline, _mesh_pipeline_layout, "defaultmesh");

		// single texture mesh shader
		auto stex_pipeline_layout_info = mesh_pipeline_layout_info;
		auto tex_set_layouts = { _vk.global_set_layout, _vk.object_set_layout, _vk.texture_set_layout };
		stex_pipeline_layout_info.setLayoutCount = tex_set_layouts.size();
		stex_pipeline_layout_info.pSetLayouts = tex_set_layouts.begin();

		VkPipelineLayout stex_pipe_layout;
		VK_CHECK(vkCreatePipelineLayout(_vk.device(), &stex_pipeline_layout_info, nullptr, &stex_pipe_layout));

		pipeline_builder._shader_stages.clear();
		pipeline_builder._pipelineLayout = stex_pipe_layout;
		pipeline_builder._shader_stages.push_back(
			vki::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, mesh_triangle_vertex));
		pipeline_builder._shader_stages.push_back(
			vki::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, textured_mesh_shader));

		auto tex_pipeline = pipeline_builder.build_pipeline(_vk.device(), _vk.renderpass);
		create_material(tex_pipeline, stex_pipe_layout, "texturedmesh");

		//cleanup
		vkDestroyShaderModule(_vk.device(), default_lit_frag, nullptr);
		vkDestroyShaderModule(_vk.device(), textured_mesh_shader, nullptr);
		vkDestroyShaderModule(_vk.device(), mesh_triangle_vertex, nullptr);
		main_delq.push_function([=]() {
			vkDestroyPipelineLayout(_vk.device(), stex_pipe_layout, nullptr);
			vkDestroyPipeline(_vk.device(), tex_pipeline, nullptr);
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
		monkey.makeup.color = glm::vec4(1.f);

		std::random_device r;
		std::default_random_engine e1(r());
		std::uniform_real_distribution<float> frandom(0, 1);

		_renderables.push_back(monkey);
		for (int x = -5; x <= 5; x++) {
			for (int y = -5; y <= 5; y++) {
				for (int z = -5; z <= 5; z++) {
					RenderObject tri;
					tri.mesh = get_mesh("triangle");
					tri.material = get_material("defaultmesh");

					float ox = frandom(e1) * 0.5f - 0.25f;
					float oy = frandom(e1) * 0.5f - 0.25f;
					float oz = frandom(e1) * 0.5f - 0.25f;
					glm::mat4 translation = glm::translate(glm::mat4{ 1.0 }, glm::vec3(x + ox, z + oz, y + oy));
					glm::mat4 scale = glm::scale(glm::mat4{ 1.0 }, glm::vec3(0.2, 0.2, 0.2));
					tri.transform = translation * scale;
					float hue = frandom(e1) * 6;
					float x = (1.f - glm::abs(glm::mod(hue, 2.f)) - 1.f);
					glm::vec3 color{};
					if (hue > 5.f) {
						color = glm::vec3(1, 0, x);
					} else if (hue > 4.f) {
						color = glm::vec3(x, 0, 1);
					} else if (hue > 3.f) {
						color = glm::vec3(0, x, 1);
					} else if (hue > 2.f) {
						color = glm::vec3(0, 1, x);
					} else if (hue > 1.f) {
						color = glm::vec3(x, 1, 0);
					} else {
						color = glm::vec3(1, x, 0);
					}
					tri.makeup.color = glm::vec4(color, 1.f);

					_renderables.push_back(tri);
				}
			}
		}
		auto sampler_info = vki::sampler_create_info(VK_FILTER_NEAREST);
		VkSampler blocky_sampler;
		vkCreateSampler(_vk.device(), &sampler_info, nullptr, &blocky_sampler);

		auto textured_mat = get_material("texturedmesh");
		VkDescriptorSetAllocateInfo alloc_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = _vk.descriptor_pool,
			.descriptorSetCount = 1,
			.pSetLayouts = &_vk.texture_set_layout,
		};
		vkAllocateDescriptorSets(_vk.device(), &alloc_info, &textured_mat->texture_set);

		VkDescriptorImageInfo image_buffer_info;
		image_buffer_info.sampler = blocky_sampler;
		image_buffer_info.imageView = _textures["empire_diffuse"].view;
		image_buffer_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet texture1 = vki::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, textured_mat->texture_set, &image_buffer_info, 0);
		vkUpdateDescriptorSets(_vk.device(), 1, &texture1, 0, nullptr);

		RenderObject map;
		map.mesh = get_mesh("empire");
		map.material = get_material("texturedmesh");
		map.transform = glm::translate(glm::mat4{ 1.0f }, glm::vec3{ 5, -10, 0 });
		_renderables.push_back(map);


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

	void zCore::init_upload_context() {
		VkFenceCreateInfo fence_create_info = {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
		};
		vkCreateFence(_vk.device(), &fence_create_info, nullptr, &_up.uploadF);
		auto command_pool_info = vki::command_pool_create_info(
			_vk.vkb_device.get_queue_index(vkb::QueueType::graphics).value());
		vkCreateCommandPool(_vk.device(), &command_pool_info, nullptr, &_up.pool);
		_up.allocator = _vk.allocator;
		_up.device = _vk.device();
		_up.graphics_queue = _vk.graphics_queue;

		main_delq.push_function([=]() {
			vkDestroyFence(_vk.device(), _up.uploadF, nullptr);
			vkDestroyCommandPool(_vk.device(), _up.pool, nullptr);
			});

	}

	void zCore::init_descriptor_set_layouts() {
		std::vector<VkDescriptorPoolSize> sizes = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10},
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10},
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10},
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10},
		};
		
		VkDescriptorPoolCreateInfo pool_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = 0,
			.maxSets = 10,
			.poolSizeCount = (u32)sizes.size(),
			.pPoolSizes = sizes.data(),
		};
		vkCreateDescriptorPool(_vk.device(), &pool_info, nullptr, &_vk.descriptor_pool);
		main_delq.push_function([=]() {
			vkResetDescriptorPool(_vk.device(), _vk.descriptor_pool, 0);
			vkDestroyDescriptorPool(_vk.device(), _vk.descriptor_pool, nullptr);
			});
		
		VkDescriptorSetLayoutBinding scene_buffer_binding = vki::descriptorset_layout_binding(
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			0
		);

		auto buffer_bindings = { scene_buffer_binding };
		VkDescriptorSetLayoutCreateInfo set_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.bindingCount = (u32)buffer_bindings.size(),
			.pBindings = buffer_bindings.begin(),
		};
		
		vkCreateDescriptorSetLayout(_vk.device(), &set_info, nullptr, &_vk.global_set_layout);


		// object set layout
		auto object_bind = vki::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
		auto object2_bind = vki::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1);
		auto object_bindings = { object_bind, object2_bind };
		VkDescriptorSetLayoutCreateInfo object_set_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.bindingCount = (u32)object_bindings.size(),
			.pBindings = object_bindings.begin(),
		};

		vkCreateDescriptorSetLayout(_vk.device(), &object_set_info, nullptr, &_vk.object_set_layout);

		auto texture_bind = vki::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
		VkDescriptorSetLayoutCreateInfo texture_set_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.bindingCount = 1,
			.pBindings = &texture_bind,
		};
		vkCreateDescriptorSetLayout(_vk.device(), &texture_set_info, nullptr, &_vk.texture_set_layout);


		main_delq.push_function([=]() {
			vkDestroyDescriptorSetLayout(_vk.device(), _vk.global_set_layout, nullptr);
			vkDestroyDescriptorSetLayout(_vk.device(), _vk.object_set_layout, nullptr);
			});
	}

	void zCore::init_descriptor_sets() {
		const size_t sceneParamBufferSize = (FRAME_OVERLAP) * pad_uniform_buffer_size(sizeof(GPUSceneData));
		
		scene_parameter_buffer = create_buffer(_vk.allocator, sceneParamBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
		main_delq.push_function([=]() {
			vmaDestroyBuffer(_vk.allocator, scene_parameter_buffer.buffer, scene_parameter_buffer.allocation);
			});


		for (auto i = 0u; i < frames.size(); i++) {
			frames[i].camera_buffer = create_buffer(_vk.allocator, sizeof(GPUCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
			main_delq.push_function([=]() {
				vmaDestroyBuffer(_vk.allocator, frames[i].camera_buffer.buffer, frames[i].camera_buffer.allocation);
				});

			VkDescriptorSetAllocateInfo alloc_info = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.pNext = nullptr,
				.descriptorPool = _vk.descriptor_pool,
				.descriptorSetCount = 1,
				.pSetLayouts = &_vk.global_set_layout,
			};

			vkAllocateDescriptorSets(_vk.device(), &alloc_info, &frames[i].global_descriptor);

			VkDescriptorSetAllocateInfo object_alloc_info = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.pNext = nullptr,
				.descriptorPool = _vk.descriptor_pool,
				.descriptorSetCount = 1,
				.pSetLayouts = &_vk.object_set_layout,
			};
			vkAllocateDescriptorSets(_vk.device(), &object_alloc_info, &frames[i].object_descriptor);

			// this is what actually sets up the link between descriptors (shader variables) and the buffers
			VkDescriptorBufferInfo scene_info = {
				.buffer = scene_parameter_buffer.buffer,
				.offset = 0,
				.range = sizeof(GPUSceneData),
			};
			
			VkDescriptorBufferInfo object_info = {
				.buffer = frames[i].object_buffer.buffer,
				.offset = 0,
				.range = sizeof(GPUObjectData) * MAX_OBJECTS,
			};

			VkDescriptorBufferInfo makeup_info = {
				.buffer = frames[i].makeup_buffer.buffer,
				.offset = 0,
				.range = sizeof(GPUMakeupData) * MAX_OBJECTS,
			};
			
			VkWriteDescriptorSet scene_write = vki::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
				frames[i].global_descriptor, &scene_info, 0);
			
			VkWriteDescriptorSet object_write = vki::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				frames[i].object_descriptor, &object_info, 0);
			
			VkWriteDescriptorSet makeup_write = vki::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				frames[i].object_descriptor, &makeup_info, 1);

			auto set_writes = {
				scene_write,
				object_write,
				makeup_write,
			};
			
			vkUpdateDescriptorSets(_vk.device(), (u32)set_writes.size(), set_writes.begin(), 0, nullptr);
			// -----------------------
		}
	}

	void vk_immediate(UploadContext& up, std::function<void(VkCommandBuffer cmd)>&& function) {
		// update for seperate context
		VkCommandBufferAllocateInfo cmd_alloc_info = vki::command_buffer_allocate_info(up.pool);
		VkCommandBuffer cmd;
		vkAllocateCommandBuffers(up.allocator->m_hDevice, &cmd_alloc_info, &cmd);
		auto begin_info = vki::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		vkBeginCommandBuffer(cmd, &begin_info);
		function(cmd);
		vkEndCommandBuffer(cmd);
		VkSubmitInfo submit = {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.pNext = nullptr,
			.waitSemaphoreCount = 0,
			.pWaitSemaphores = nullptr,
			.pWaitDstStageMask = 0,
			.commandBufferCount = 1,
			.pCommandBuffers = &cmd,
			.signalSemaphoreCount = 0,
			.pSignalSemaphores = nullptr,
		};

		vkQueueSubmit(up.graphics_queue, 1, &submit, up.uploadF);
		vkWaitForFences(up.device, 1, &up.uploadF, true, 999'999'999'999);
		vkResetFences(up.device, 1, &up.uploadF);

		vkResetCommandPool(up.device, up.pool, 0);
	}

	void zCore::init_imgui() {
		VkDescriptorPoolSize pool_sizes[] = {
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 },
		};

		VkDescriptorPoolCreateInfo pool_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
			.maxSets = 1000,
			.poolSizeCount = (u32)std::size(pool_sizes),
			.pPoolSizes = pool_sizes,
		};

		VkDescriptorPool imgui_pool;
		VK_CHECK(vkCreateDescriptorPool(_vk.device(), &pool_info, nullptr, &imgui_pool));
		ImGui::CreateContext();
		ImGui_ImplGlfw_InitForVulkan(_window.handle, true);
		ImGui_ImplVulkan_InitInfo init_info = {
			.Instance = _vk.instance(),
			.PhysicalDevice = _vk.vkb_device.physical_device.physical_device,
			.Device = _vk.device(),
			.Queue = _vk.graphics_queue,
			.DescriptorPool = imgui_pool,
			.MinImageCount = 3,
			.ImageCount = 3,
			.MSAASamples = VK_SAMPLE_COUNT_1_BIT,
			.CheckVkResultFn = [](VkResult err) {
				if (!err) return;
				DBG("vk error: " << err);
				},
		};

		ImGui_ImplVulkan_Init(&init_info, _vk.renderpass);
		vk_immediate(_up, [=](VkCommandBuffer cmd) {
			ImGui_ImplVulkan_CreateFontsTexture(cmd);
			});

		ImGui_ImplVulkan_DestroyFontUploadObjects();
		main_delq.push_function([=]() {
			vkDestroyDescriptorPool(_vk.device(), imgui_pool, nullptr);
			ImGui_ImplVulkan_Shutdown();
			});

	}

	// INPUT

	void zCore::set_cursor_absolute(bool absolute) {
		this->cursor_use_absolute_position = absolute;
		u32 cursor_mode;
		if (this->cursor_use_absolute_position) {
			cursor_mode = GLFW_CURSOR_NORMAL;
			ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
		} else {
			cursor_mode = GLFW_CURSOR_DISABLED;
			ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
		}
		glfwSetInputMode(_window.handle, GLFW_CURSOR, cursor_mode);
	}

	void zCore::_glfw_key_callback_caller(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods) {
		auto io = ImGui::GetIO();
		if (!io.WantCaptureKeyboard) {
			zCore* zcore = (zCore*)glfwGetWindowUserPointer(window);
			zcore->_glfw_key_callback(window, key, scancode, action, mods);
		}
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

	void zCore::_glfw_mouse_position_callback_caller(GLFWwindow* window, double x, double y) {
		//ImGuiIO& io = ImGui::GetIO(); 
		//DBG("x: " << ImGui::GetIO().MousePos.x << "y: " << ImGui::GetIO().MousePos.y << "");

		zCore* zcore = (zCore*)glfwGetWindowUserPointer(window);
		zcore->_glfw_mouse_position_callback(window, x, y);
	}

	void zCore::_glfw_mouse_position_callback(GLFWwindow* window, double x, double y) {
		if (cursor_use_absolute_position) {
			// schedule UI to update, but I dont have UI yet..
		} else {
			glm::vec2 mouse_nowpos = { x, y };
			mouse_delta += mouse_nowpos - _mouse_old_pos;
		}

		_mouse_old_pos = { x, y };
	}

	void zCore::process_key_inputs() {
		std::set<InputAction> held_actions;
		for (auto keyi : key_inputs) {
			if (keyi.condition == HOLD && is_key_held(keyi.key)) {
				held_actions.insert(keyi.action);
			}
		}

		glm::vec2 movement_dir{ 0.f,0.f };
		if (held_actions.contains(InputAction::MOVE_FORWARD)) {
			movement_dir += glm::vec2{ 0.f, 1.f };
		}
		if (held_actions.contains(InputAction::MOVE_BACK)) {
			movement_dir += glm::vec2{ 0.f, -1.f };
		}
		if (held_actions.contains(InputAction::MOVE_STRAFE_LEFT)) {
			movement_dir += glm::vec2{ -1.f, 0.f };
		}
		if (held_actions.contains(InputAction::MOVE_STRAFE_RIGHT)) {
			movement_dir += glm::vec2{ 1.f, 0.f };
		}

		//DBG("cpos " << _camera._pos);
		//DBG("cfwd " << _camera.forward());
		if (movement_dir != glm::vec2{ 0.f, 0.f }) {
			auto direction = glm::normalize(_camera.right() * movement_dir.x + _camera.forward() * movement_dir.y);
			float speed = 12.f;
			_camera.apply_movement(TICK_DT * speed * direction);
		}

		float fly_dir = 0.f;
		if (held_actions.contains(InputAction::MOVE_FLY_UP)) {
			fly_dir -= 1.f;
		} 
		if (held_actions.contains(InputAction::MOVE_FLY_DOWN)) {
			fly_dir += 1.f;
		}
		if (fly_dir != 0.f) {
			float fly_speed = 6.f;
			_camera.apply_movement(TICK_DT * fly_speed * fly_dir * _camera.up());
		}


	}

	void zCore::process_mouse_inputs() {
		//DBG("dx " << mouse_delta.x << " dy " << mouse_delta.y);
		//DBG(" x " << _camera._pos.x << "  y " << _camera._pos.y << "  z " << _camera._pos.z);
		//DBG("phi " << _camera.phi << " theta " << _camera.theta);

		auto invert_cam_factor = invert_camera ? -1.f : 1.f;
		auto sensitivity_delta = mouse_delta_sens * mouse_delta;
		_camera.apply_rotation(sensitivity_delta.y * invert_cam_factor, sensitivity_delta.x);
		mouse_delta = glm::vec2(0.f);
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
		return frames[current_frame_idx()];
	}

	u32 zCore::current_frame_idx() {
		return frame_counter % FRAME_OVERLAP;
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

		Mesh lost_empire{};
		lost_empire.load_from_obj("../assets/lost_empire.obj");
		upload_mesh(lost_empire);
		_meshes["empire"] = lost_empire;
	}

	void zCore::upload_mesh(Mesh& mesh) {
		const size_t buffer_size = mesh._vertices.size() * sizeof(mesh._vertices[0]);

		VkBufferCreateInfo staging_info = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext = nullptr,
			.size = (u32)buffer_size,
			.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		};

		VmaAllocationCreateInfo staging_alloc_info = {
			.usage = VMA_MEMORY_USAGE_CPU_ONLY,
		};

		AllocBuffer staging_buffer;
		VK_CHECK(vmaCreateBuffer(_vk.allocator, &staging_info, &staging_alloc_info, &staging_buffer.buffer, &staging_buffer.allocation, nullptr));
		
		void* data;
		vmaMapMemory(_vk.allocator, staging_buffer.allocation, &data);
		memcpy(data, mesh._vertices.data(), buffer_size);
		vmaUnmapMemory(_vk.allocator, staging_buffer.allocation);

		if (_vk.allocator->IsIntegratedGpu()) {
			// we dont need to copy, as cpu and gpu visible memory are usually the same
			mesh._vertex_buffer = staging_buffer;
		} else {
			// need to copy from cpu to gpu
			VkBufferCreateInfo buffer_info = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.size = (u32)buffer_size,
				.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			};

			VmaAllocationCreateInfo vma_alloc_info = {
				.usage = VMA_MEMORY_USAGE_GPU_ONLY,
			};

			VK_CHECK(vmaCreateBuffer(_vk.allocator, &buffer_info, &vma_alloc_info, &mesh._vertex_buffer.buffer, &mesh._vertex_buffer.allocation, nullptr));

			// copy from staging to vertex
			vk_immediate(_up, [=](VkCommandBuffer cmd) {
				VkBufferCopy copy;
				copy.dstOffset = 0;
				copy.srcOffset = 0;
				copy.size = buffer_size;
				vkCmdCopyBuffer(cmd, staging_buffer.buffer, mesh._vertex_buffer.buffer, 1, &copy);
				});

			vmaDestroyBuffer(_vk.allocator, staging_buffer.buffer, staging_buffer.allocation);
		}

		main_delq.push_function([=]() {
			vmaDestroyBuffer(_vk.allocator, mesh._vertex_buffer.buffer, mesh._vertex_buffer.allocation);
			});

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

	AllocBuffer create_buffer(VmaAllocator& allocator, size_t alloc_size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage) {
		VkBufferCreateInfo buffer_info = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext = nullptr,
			.size = alloc_size,
			.usage = usage,
		};

		VmaAllocationCreateInfo vma_info = {
			.usage = memory_usage,
		};

		AllocBuffer buf{};
		VK_CHECK(vmaCreateBuffer(allocator, &buffer_info, &vma_info, &buf.buffer, &buf.allocation, nullptr));
		return buf;
	}

	void zCore::setup_draw() {
		_df.old_frame_start = std::chrono::steady_clock::now();
		_df.global_current_time = 0.0;
	}

	void zCore::draw() {
		
		auto monitor = glfwGetPrimaryMonitor();
		auto refresh_rate = glfwGetVideoMode(monitor)->refreshRate;
		auto dt = 1.f / 100.f;
		vkResetFences(_vk.device(), 1, &current_frame().renderF);

		uint32_t swapchain_image_idx;
		auto acquire_result = vkAcquireNextImageKHR(_vk.device(), _window.swapchain(), 0, current_frame().presentS, nullptr, &swapchain_image_idx);

		if (acquire_result == VK_SUCCESS) {
			// we have an image to render to
			VK_CHECK(vkResetCommandBuffer(current_frame().buf, 0));
			VkCommandBufferBeginInfo cmd_begin_info = vki::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

			VK_CHECK(vkBeginCommandBuffer(current_frame().buf, &cmd_begin_info));
			VkClearValue clear_value;
			float flash = (float)abs(sin(_df.global_current_time));
			clear_value.color = { {0.2f, 0.2f, 1.f, 1.f} };

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
			viewport.height = (float)_window.extent().height;
			viewport.width = (float)_window.extent().width;
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

			// -- camera

			_camera.aspect = viewport.width / viewport.height;

			//

			draw_objects(current_frame().buf, std::span<RenderObject>(_renderables.data(), _renderables.size()));

			auto imgui_draw_data = ImGui::GetDrawData();
			if (imgui_draw_data != nullptr) {
				ImGui_ImplVulkan_RenderDrawData(imgui_draw_data, current_frame().buf);
			}
			// --

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

	void bind_descriptors(VkCommandBuffer cmd, PerFrameData& frame, Material* material, u32 scene_buffer_offset) {

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, material->pipeline);

		auto offsets = { scene_buffer_offset };
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, material->pipeline_layout, 0, 1, &frame.global_descriptor, (u32)offsets.size(), offsets.begin());

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, material->pipeline_layout, 1, 1, &frame.object_descriptor, 0, nullptr);

		if (material->texture_set != VK_NULL_HANDLE) {
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, material->pipeline_layout, 2, 1, &material->texture_set, 0, nullptr);
		}

	}

	void bind_mesh(VkCommandBuffer cmd, Mesh* mesh) {
		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(cmd, 0, 1, &mesh->_vertex_buffer.buffer, &offset);
	}

	void zCore::draw_objects(VkCommandBuffer cmd, std::span<RenderObject> render_objects) {

		// -- pushing around data into buffers

		glm::mat4 view = _camera.view();
		glm::mat4 projection = _camera.projection();
		GPUCameraData camera_data = {
			.view = view,
			.proj = projection,
			.viewproj = projection * view,
		};

		u32 scene_buffer_offset = pad_uniform_buffer_size(sizeof(scene_parameters)) * current_frame_idx();
		scene_parameters.ambient_color = { (sin(_df.global_current_time)+1.f)/2.f,0,(cos(_df.global_current_time))+1.f/2.f,1 };
		scene_parameters.camera = camera_data;
		{ // CPUTOGPU -- scene buffer copy
			char* data;
			vmaMapMemory(_vk.allocator, scene_parameter_buffer.allocation, (void**)&data);
			auto offset_ptr = data + scene_buffer_offset;
			memcpy(offset_ptr, &scene_parameters, sizeof(scene_parameters));
			vmaUnmapMemory(_vk.allocator, scene_parameter_buffer.allocation);
		}

		
		void* object_data; // CPUTOGPU object buffer copy 
		vmaMapMemory(_vk.allocator, current_frame().object_buffer.allocation, &object_data);
		GPUObjectData* objectSSBO = (GPUObjectData*)object_data;
		for (size_t i = 0; i < render_objects.size(); i++) {
			RenderObject& object = render_objects.data()[i];
			objectSSBO[i].model_matrix = object.transform;
		}
		vmaUnmapMemory(_vk.allocator, current_frame().object_buffer.allocation);

		void* makeup_data; // CPUTOGPU makeup buffer copy 
		vmaMapMemory(_vk.allocator, current_frame().makeup_buffer.allocation, &makeup_data);
		GPUMakeupData* makeupSSBO = (GPUMakeupData*)makeup_data;
		for (size_t i = 0; i < render_objects.size(); i++) {
			RenderObject& object = render_objects[i];
			makeupSSBO[i].color = object.makeup.color;
		}
		vmaUnmapMemory(_vk.allocator, current_frame().makeup_buffer.allocation);

		if (render_objects.empty()) return;
		assert(render_objects.size() > 0);

		// -- prepass
		std::vector<IndirectBatch> draws;
		bool first = true;
		IndirectBatch indirect_draw{
			.mesh = render_objects[0].mesh,
			.material = render_objects[0].material,
			.first = 0,
			.count = 1,
		};
		draws.push_back(indirect_draw);

		for (u32 i = 1; i < render_objects.size(); i++) {
			if (render_objects[i].material == draws.back().material && render_objects[i].mesh == draws.back().mesh) {
				// same
				draws.back().count++;
			} else {
				// different
				IndirectBatch diff_draw{
					.mesh = render_objects[i].mesh,
					.material = render_objects[i].material,
					.first = i,
					.count = 1,
				};
				draws.push_back(diff_draw);
			}
		}


		      
		// -- actual draw code

		for (auto& draw : draws)  {
			bind_descriptors(cmd, current_frame(), draw.material, scene_buffer_offset);
			bind_mesh(cmd, draw.mesh);
			for (u32 i = 0; i < draw.count; i++) {
				vkCmdDraw(cmd, (u32)draw.mesh->_vertices.size(), 1, 0, draw.first + i);
			}

		}

	}

	// APP
	void zCore::app_loop() {

		auto old_tick = std::chrono::steady_clock::now();
		auto tick_acc = 0.f;
		auto dt = TICK_DT;
		
		boost::circular_buffer<float> frame_times(250);
		

		while (!glfwWindowShouldClose(_window.handle)) {
			glfwPollEvents();
			if (die) {
				DBG("time to die");
				return;
			}
			auto now_tick = std::chrono::steady_clock::now();
			std::chrono::duration<float> tick_fdiff = now_tick - old_tick;
			tick_acc += tick_fdiff.count();

			while (tick_acc > dt) {
				// tick loop
				tick_acc -= dt;
				process_key_inputs();
				process_mouse_inputs();
				_camera.tick();
			}

			// -- drawing and animation

			if (vkGetFenceStatus(_vk.device(), current_frame().renderF) == VK_SUCCESS) 	{ // actual rendering
				auto start_frame = std::chrono::high_resolution_clock::now();
				auto anim_dt = start_frame - _df.old_frame_start;
				auto anim_dt_float = std::chrono::duration<float>(anim_dt).count();
				frame_times.push_back(anim_dt_float);
				_df.global_current_time += anim_dt_float;

				// -- imgui
				ImGui_ImplVulkan_NewFrame();
				ImGui_ImplGlfw_NewFrame();
				ImGui::NewFrame();
				ImGui::Begin("My first tool", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
				ImGui::PlotLines("", frame_times.linearize(), (i32)frame_times.size(), 0, "Frame DT", 0.f, 0.1f, ImVec2(250, 100), 4);
				auto avg_ft = std::accumulate(frame_times.begin(), frame_times.end(), 0.f)/frame_times.size();
				
				ImGui::Text("_gt %f", (float)_df.global_current_time);
				ImGui::Text("_frameidx %u", current_frame_idx());
				ImGui::Text("avg frametime %f", avg_ft);
				ImGui::End();
				ImGui::Render();
				
				// -- vulkan
				draw();


				_df.old_frame_start = start_frame;
			}


			old_tick = now_tick;
			std::this_thread::yield();
		}
		this->cleanup();
	}

	size_t zCore::pad_uniform_buffer_size(size_t original_size) {
		// Calculate required alignment based on minimum device offset alignment
		size_t min_ubo_alignment = _vk.gpu_properties.limits.minUniformBufferOffsetAlignment;
		size_t aligned_size = original_size;
		if (min_ubo_alignment > 0) {
			aligned_size = (aligned_size + min_ubo_alignment - 1) & ~(min_ubo_alignment - 1);
		}
		return aligned_size;
	}

	void zCore::load_images() {
		Texture lost_empire;
		vku::load_image_from_file(_up, "../assets/lost_empire-RGBA.png", lost_empire.image);
		VkImageViewCreateInfo image_info = vki::imageview_create_info(VK_FORMAT_R8G8B8A8_SRGB, lost_empire.image.image, VK_IMAGE_ASPECT_COLOR_BIT);
		vkCreateImageView(_vk.device(), &image_info, nullptr, &lost_empire.view);
		_textures["empire_diffuse"] = lost_empire;
	}
}