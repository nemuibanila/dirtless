#include "zebralib.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <random>
#include <magic_enum.h>
#include <numeric>

#include "vk_mem_alloc.h"
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <VkBootstrap.h>

#include "vki.h"
#include "g_types.h"
#include "g_pipeline.h"
#include "g_mesh.h"
#include "g_vec.h"
#include "g_texture.h"
#include "g_buffer.h"
#include "g_vku.h"
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
			.key = {GLFW_KEY_K},
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
			.key = Key{GLFW_KEY_SPACE},
			.condition = KeyCondition::HOLD,
			.action = InputAction::MOVE_FLY_UP,
		};
		key_inputs.push_back(up);

		KeyInput down{
			.key = Key{GLFW_KEY_C},
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


		auto sampler_info = vki::sampler_create_info(VK_FILTER_NEAREST);
		vkCreateSampler(_up.device, &sampler_info, nullptr, &_vk.default_sampler);
		main_delq.push_function([this] {
			vkDestroySampler(_up.device, _vk.default_sampler, nullptr);
			});

		DBG("swapchain");
		if (!this->init_swapchain()) return false;

		DBG("per frame data");
		if (!this->init_swapchain_per_frame_data()) return false; 

		DBG("render pass");
		if (!this->init_default_renderpass()) return false; // ???

		DBG("framebuffers");
		if (!this->init_framebuffers()) return false;

		DBG("pipelines/shaders");
		init_descriptor_set_layouts();

		init_per_frame_data();
		if (!this->init_pipelines()) return false;

		init_renderer();

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

	void zCore::init_renderer() {
		main_delq.push_function([this] {
			render::clear_buffers(renderer, _up);
			for (auto& buf : assets.t_meshes) {
				vmaDestroyBuffer(_up.allocator, buf.second.vertices.buffer, buf.second.vertices.allocation);
			}
			for (auto& tex : assets.t_textures) {
				destroy_texture(_up, tex.second);
			}
			for (auto& mat : assets.t_materials) {
				vkDestroyPipelineLayout(_up.device, mat.second.pipeline_layout, nullptr);
				vkDestroyPipeline(_up.device, mat.second.pipeline, nullptr);
			}

			});
	}

	bool zCore::init_per_frame_data() {
		//for (auto i = 0u; i < FRAME_OVERLAP; i++) {
		//	auto& frame = frames[i];
		//}



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
			VK_CHECK(vkCreateCommandPool(_up.device, &poolinfo, nullptr, &frames[i].pool));
			auto cmd_info = vki::command_buffer_allocate_info(
				frames[i].pool);

			VK_CHECK(vkAllocateCommandBuffers(_up.device, &cmd_info, &frames[i].buf));
			VK_CHECK(vkCreateFence(_up.device, &fence_info, nullptr, &frames[i].renderF));
			VK_CHECK(vkCreateSemaphore(_up.device, &semaphore_info, nullptr, &frames[i].presentS));
			VK_CHECK(vkCreateSemaphore(_up.device, &semaphore_info, nullptr, &frames[i].renderS));

			swapchain_delq.push_function([this, i]() {
				vkDestroyCommandPool(_up.device, frames[i].pool, nullptr);
				vkDestroyFence(_up.device, frames[i].renderF, nullptr);
				vkDestroySemaphore(_up.device, frames[i].renderS, nullptr);
				vkDestroySemaphore(_up.device, frames[i].presentS, nullptr);
				});

		}
		return true;
	}

	// make more generic, need multiple passes
	bool zCore::init_default_renderpass() {

		// forward pass
		{
			auto color_attachment = vki::attachment_description(
				_vk.screen_texture.format,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR,
				VK_ATTACHMENT_STORE_OP_STORE);

			auto depth_attachment = vki::attachment_description(
				_vk.depth_texture.format,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR,
				VK_ATTACHMENT_STORE_OP_STORE);

			

			VkAttachmentReference color_attachment_ref = {
				.attachment = 0,
				.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			};
			VkAttachmentReference depth_attachment_ref = {
				.attachment = 1,
				.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			};

			auto color_attachment_references = { color_attachment_ref };

			VkSubpassDescription subpass = {
				.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
				.colorAttachmentCount = (u32)color_attachment_references.size(),
				.pColorAttachments = color_attachment_references.begin(),
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

			VK_CHECK(vkCreateRenderPass(_up.device, &render_pass_info, nullptr, &_vk.forward_renderpass));
			swapchain_delq.push_function([this]() {
				vkDestroyRenderPass(_up.device, _vk.forward_renderpass, nullptr);
				});
		}

		// copy pass 
		{
			auto color_attachment = vki::attachment_description(
				_window.vkb_swapchain.image_format,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				VK_ATTACHMENT_STORE_OP_STORE);

			VkAttachmentReference color_attachment_ref = {
				.attachment = 0,
				.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			};

			VkSubpassDescription subpass = {
				.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
				.colorAttachmentCount = 1,
				.pColorAttachments = &color_attachment_ref,
				.pDepthStencilAttachment = nullptr,
			};

			VkRenderPassCreateInfo render_pass_info = {
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
				.attachmentCount = 1,
				.pAttachments = &color_attachment,
				.subpassCount = 1,
				.pSubpasses = &subpass,
			};

			VK_CHECK(vkCreateRenderPass(_up.device, &render_pass_info, nullptr, &_vk.copy_pass));
			swapchain_delq.push_function([this]() {
				vkDestroyRenderPass(_up.device, _vk.copy_pass, nullptr);
				});
		}

		return true;
	}



	// as render pass becomes more generic, so does this
	bool zCore::init_framebuffers() {

		i32 w, h;
		w = _window.extent().width;
		h = _window.extent().height;

		// forward pass
		{
			auto forward_info = vki::framebuffer_info(_vk.forward_renderpass, _window.extent());
			auto attachments = { _vk.screen_texture.view, _vk.depth_texture.view };
			forward_info.attachmentCount = attachments.size();
			forward_info.pAttachments = attachments.begin();
			VK_CHECK(vkCreateFramebuffer(_up.device, &forward_info, nullptr, &_vk.forward_framebuffer));
			swapchain_delq.push_function([this]() {
				vkDestroyFramebuffer(_up.device, _vk.forward_framebuffer, nullptr);
				});

		}
		// final pass to copy to screen
		const u32 swapchain_imagecount = (u32)_vk.image_views.size();
		auto swapchain_imageviews = _vk.image_views;
		_vk.framebuffers = std::vector<VkFramebuffer>(_vk.image_views.size());

		auto final_framebuffer_info = vki::framebuffer_info(_vk.copy_pass, _window.extent());
		for (auto i = 0u; i < swapchain_imagecount; i++) {
			final_framebuffer_info.attachmentCount = 1;
			final_framebuffer_info.pAttachments = &swapchain_imageviews[i];
			VK_CHECK(vkCreateFramebuffer(_up.device, &final_framebuffer_info, nullptr, &_vk.framebuffers[i]));
			swapchain_delq.push_function([this, i]() {
				vkDestroyFramebuffer(_up.device, _vk.framebuffers[i], nullptr);
				});
		}


		return true;
	}


	// ok 02.09.2021
	bool zCore::create_window() {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		_window.handle = glfwCreateWindow(1280, 720, "Zebra", NULL, NULL);
		if (_window.handle == nullptr) {
			DBG("Failed: nullptr");
			return false;
		}


		return true;
	}

	void destroy_texture(UploadContext& up, Texture tex) {
		vkDestroyImageView(up.device, tex.view, nullptr);
		vmaDestroyImage(up.allocator, tex.image, tex.allocation);
	}

	// ok 02.09.2021 -> touches renderpass
	bool zCore::init_swapchain() {
		vkDeviceWaitIdle(_up.device);

		vkb::SwapchainBuilder swapchain_builder{ _vk.vkb_device };
		auto swap_ret = swapchain_builder
			.use_default_present_mode_selection()
			.build();
		if (!swap_ret) {
			DBG("creation failed. " << swap_ret.error().message());
			return false;
		}

		_window.vkb_swapchain = swap_ret.value();

		// swapchain present textures
		{
			_vk.images = _window.vkb_swapchain.get_images().value();
			_vk.image_views = _window.vkb_swapchain.get_image_views().value();
			swapchain_delq.push_function([this]() {
				for (auto i = 0u; i < _vk.image_views.size(); i++) {
					vkDestroyImageView(_up.device, _vk.image_views[i], nullptr);
				}
				vkb::destroy_swapchain(_window.vkb_swapchain);
				});
		}
		// depth texture
		{
			VkExtent3D depth_image_extent = {
				.width = _window.extent().width,
				.height = _window.extent().height,
				.depth = 1,
			};
			VkImageCreateInfo depth_image_info = vki::image_create_info(VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, depth_image_extent);
			create_gpu_texture(_up, depth_image_info, VK_IMAGE_ASPECT_DEPTH_BIT, _vk.depth_texture);

			swapchain_delq.push_function([this]() {
				destroy_texture(_up, _vk.depth_texture);
				});
		}
		// screen texture
		{
			VkExtent3D render_image_extent = {
				.width = _window.extent().width,
				.height = _window.extent().height,
				.depth = 1,
			};

			auto screen_texture_info = vki::image_create_info(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, render_image_extent);
			create_gpu_texture(_up, screen_texture_info, VK_IMAGE_ASPECT_COLOR_BIT, _vk.screen_texture);

			swapchain_delq.push_function([this]() {
				destroy_texture(_up, _vk.screen_texture);
				});

		}


		return true;
	}

	// ok 02.09.2021 -> touches renderpass
	bool zCore::recreate_swapchain() {

		vkDeviceWaitIdle(_up.device);
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
			.request_validation_layers()
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

		glfwCreateWindowSurface(_vk.vkb_instance.instance, _window.handle, nullptr, &_window.surface);


		vkb::PhysicalDeviceSelector selector{ _vk.vkb_instance };
		auto phys_ret = selector
			.set_surface(_window.surface)
			.set_minimum_version(1, 2)
			.add_desired_extension("VK_KHR_shader_draw_parameters")
			.prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
			.select();
		if (!phys_ret) {
			DBG("physical device selection err: " << phys_ret.error().message());
			return false;
		}

		auto physical_device = phys_ret.value();
		vkGetPhysicalDeviceFeatures(physical_device.physical_device, &physical_device.features);

		vkb::DeviceBuilder device_builder{ physical_device };
		auto dev_ret = device_builder
			.build();
		if (!dev_ret) {
			DBG("device err: " << dev_ret.error().message());
			return false;
		}
		_vk.vkb_device = dev_ret.value();
		DBG("multidrawindirect: " << _vk.vkb_device.physical_device.features.multiDrawIndirect);

		auto graphics_queue_ret = _vk.vkb_device.get_queue(vkb::QueueType::graphics);
		if (!graphics_queue_ret) {
			DBG("no graphics queue: " << graphics_queue_ret.error().message());
			return false;
		}
		_vk.graphics_queue = graphics_queue_ret.value();

		VmaAllocatorCreateInfo allocate_info = {
			.physicalDevice = _vk.vkb_device.physical_device.physical_device,
			.device = _vk.vkb_device.device,
			.instance = _vk.vkb_instance.instance,
		};
		vmaCreateAllocator(&allocate_info, &_vk.allocator);
		main_delq.push_function([this]() {
			vmaDestroyAllocator(_vk.allocator);
			});

		vkGetPhysicalDeviceProperties(_vk.vkb_device.physical_device.physical_device, &_vk.gpu_properties);
		DBG("gpu minimum buffer alignment: " << _vk.gpu_properties.limits.minUniformBufferOffsetAlignment);
		init_upload_context();

		DBG("complete and ready to use.");
		return true;
	}

	// this is testing only 02.09.2021, needs generify
	bool zCore::init_pipelines() {

		VkShaderModule default_lit_frag;
		VkShaderModule textured_mesh_shader;
		VkShaderModule mesh_triangle_vertex;
		VkShaderModule blit_fragment;
		VkShaderModule fullscreen_vertex;

		load_shader_module("../shaders/default_lit.frag.spv", &default_lit_frag);
		load_shader_module("../shaders/tri_mesh.vert.spv", &mesh_triangle_vertex);
		load_shader_module("../shaders/textured_lit.frag.spv", &textured_mesh_shader);
		load_shader_module("../shaders/blit.frag.spv", &blit_fragment);
		load_shader_module("../shaders/fullscreen.vert.spv", &fullscreen_vertex);

		PipelineBuilder pipeline_builder;


		std::array push_constants = { 
			VkPushConstantRange{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants)}
		};

		// these layouts could come from reflection
		// GLOBAL
		FatSetLayout scene_set;
		scene_set
			.add_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

		FatSetLayout object_set;
		object_set
			.add_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);

		// TEXTURE
		FatSetLayout texture_set;
		texture_set
			.add_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT );
		std::array mesh_fat_sets = {
			scene_set,
			object_set,
			texture_set,
		};

		std::cout << mesh_fat_sets[1].bindings[0].descriptorType;
		auto mesh_layout = create_pipeline_layout<DefaultFatSize>(_up.device, _vk.layout_cache, std::span(mesh_fat_sets), std::span(push_constants));

		// mesh color shader
		auto vertex_description = P3N3C3U2::get_vertex_description();
		auto mesh_pipeline = pipeline_builder
			.set_defaults()
			.depth(true, true, VK_COMPARE_OP_LESS_OR_EQUAL)
			.set_vertex_format(vertex_description)
			.set_layout(mesh_layout)
			.clear_shaders()
			.add_shader(VK_SHADER_STAGE_VERTEX_BIT, mesh_triangle_vertex)
			.add_shader(VK_SHADER_STAGE_FRAGMENT_BIT, default_lit_frag)
			.build_pipeline(_up.device, _vk.forward_renderpass);

		auto defaultmesh_fk = render::insert_material(assets, {
			.texture_set = VK_NULL_HANDLE,
			.pipeline = mesh_pipeline,
			.pipeline_layout = mesh_layout,
			});

		render::name_handle(assets, "defaultmesh", defaultmesh_fk);

		// single texture mesh shader
		std::array tex_fat_sets = {
			scene_set,
			object_set,
			texture_set,
		};

		std::array<VkPushConstantRange, 0> empty_range = {};
		auto stex_pipe_layout = create_pipeline_layout<DefaultFatSize>(_up.device, _vk.layout_cache, std::span(tex_fat_sets), std::span(empty_range));

		// mesh texture shader
		auto tex_pipeline = pipeline_builder
			.set_layout(stex_pipe_layout)
			.clear_shaders()
			.add_shader(VK_SHADER_STAGE_VERTEX_BIT, mesh_triangle_vertex)
			.add_shader(VK_SHADER_STAGE_FRAGMENT_BIT, textured_mesh_shader)
			.build_pipeline(_up.device, _vk.forward_renderpass);

		auto tmesh_fk = render::insert_material(assets, { VK_NULL_HANDLE, tex_pipeline, stex_pipe_layout });
		render::name_handle(assets, "texturedmesh", tmesh_fk);

		std::array blit_fat_sets = {
			texture_set,
		};

		VkPipelineLayout blit_pipe_layout = create_pipeline_layout<DefaultFatSize>(_up.device, _vk.layout_cache, std::span(blit_fat_sets), std::span(empty_range));
		
		// fullscreen blit
		auto blit_pipeline = pipeline_builder
			.no_vertex_format()
			.depth(false, false, VK_COMPARE_OP_ALWAYS)
			.clear_shaders()
			.add_shader(VK_SHADER_STAGE_VERTEX_BIT, fullscreen_vertex)
			.add_shader(VK_SHADER_STAGE_FRAGMENT_BIT, blit_fragment)
			.set_layout(blit_pipe_layout)
			.build_pipeline(_up.device, _vk.copy_pass);

		auto blit_fk = render::insert_material(assets, { VK_NULL_HANDLE, blit_pipeline, blit_pipe_layout });
		render::name_handle(assets, "blit", blit_fk);

		//cleanup
		vkDestroyShaderModule(_up.device, default_lit_frag, nullptr);
		vkDestroyShaderModule(_up.device, textured_mesh_shader, nullptr);
		vkDestroyShaderModule(_up.device, mesh_triangle_vertex, nullptr);
		vkDestroyShaderModule(_up.device, fullscreen_vertex, nullptr);
		vkDestroyShaderModule(_up.device, blit_fragment, nullptr);


		return true;
	}

	// generify, scene struct
	void zCore::init_scene() {
		render::RenderObject monkey;
		monkey.mesh_fk = assets.t_names["monkey"];
		monkey.material_fk = assets.t_names["defaultmesh"];
		monkey.obj.model_matrix = glm::mat4{ 1.0f };
		monkey.obj.color = glm::vec4(1.f);

		std::random_device r;
		std::default_random_engine e1(r());
		std::uniform_real_distribution<float> frandom(0, 1);

		add_renderable(renderer, monkey, true);
		for (float x = -20; x <= 20; x+=1.00001f) {
			for (float y = -12; y <= 12; y+=1.00001f) {
				for (float z = -20; z <= 20; z+=1.00001f) {
					render::RenderObject tri;
					tri.mesh_fk = assets.t_names["triangle"];
					tri.material_fk = assets.t_names["defaultmesh"];

					float ox = frandom(e1) * 0.5f - 0.25f;
					float oy = frandom(e1) * 0.5f - 0.25f;
					float oz = frandom(e1) * 0.5f - 0.25f;
					glm::mat4 translation = glm::translate(glm::mat4{ 1.0 }, glm::vec3(2.0f*x + ox, 2.0f*z + oz, 2.0f*y + oy));
					glm::mat4 scale = glm::scale(glm::mat4{ 1.0 }, glm::vec3(0.5f, 0.5f, 0.5f));
					tri.obj.model_matrix = translation * scale;
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
					tri.obj.color = glm::vec4(color, 1.f);

					add_renderable(renderer, tri, true);
				}
			}
		}


		VkDescriptorSetLayout texture_set_layout;
		VkDescriptorSet texture_set;

		VkDescriptorImageInfo image_buffer_info;
		image_buffer_info.sampler = _vk.default_sampler;
		image_buffer_info.imageView = assets.t_textures[assets.t_names["empire_diffuse"]].view;
		image_buffer_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		auto textured_mat = assets.t_names["texturedmesh"];
		DescriptorBuilder::begin(_up.device, _vk.descriptor_pool, _vk.layout_cache)
			.bind_image(0, image_buffer_info, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.build(texture_set, texture_set_layout);
		assets.t_materials[textured_mat].texture_set = texture_set;

		render::RenderObject map;
		map.mesh_fk = assets.t_names["empire_mesh"];
		map.material_fk = assets.t_names["texturedmesh"];
		map.obj.color = glm::vec4(1.f);
		map.obj.model_matrix = glm::translate(glm::mat4{ 1.0f }, glm::vec3{ 5, -10, 0 });
		add_renderable(renderer, map, true);
	}

	// ok 02.09.2021
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

	// ok for now 02.09.2021, for multithread loading, one uploadcontext per thread
	void zCore::init_upload_context() {
		VkFenceCreateInfo fence_create_info = {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
		};
		_up.device = _vk.vkb_device.device;
		vkCreateFence(_up.device, &fence_create_info, nullptr, &_up.uploadF);
		auto command_pool_info = vki::command_pool_create_info(
			_vk.vkb_device.get_queue_index(vkb::QueueType::graphics).value());
		vkCreateCommandPool(_up.device, &command_pool_info, nullptr, &_up.pool);
		_up.allocator = _vk.allocator;

		_up.graphics_queue = _vk.graphics_queue;

		main_delq.push_function([this]() {
			vkDestroyFence(_up.device, _up.uploadF, nullptr);
			vkDestroyCommandPool(_up.device, _up.pool, nullptr);
			});

	}

	// yes 18.09.2021
	void zCore::init_descriptor_set_layouts() {
		std::vector<VkDescriptorPoolSize> sizes = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100},
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100},
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100},
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100},
		};
		

		VkDescriptorPoolCreateInfo pool_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = 0,
			.maxSets = 10,
			.poolSizeCount = (u32)sizes.size(),
			.pPoolSizes = sizes.data(),
		};
		vkCreateDescriptorPool(_up.device, &pool_info, nullptr, &_vk.descriptor_pool);
		main_delq.push_function([this]() {
			vkResetDescriptorPool(_up.device, _vk.descriptor_pool, 0);
			vkDestroyDescriptorPool(_up.device, _vk.descriptor_pool, nullptr);
		});
		


		main_delq.push_function([this]() {
			_vk.layout_cache.destroy_cached(_up.device);
			});
	}

	void zCore::init_descriptor_sets() {

		for (auto i = 0u; i < frames.size(); i++) {
			std::vector<VkDescriptorPoolSize> sizes = {
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2500},
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2500},
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
			};
			VkDescriptorPoolCreateInfo pool_info = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
				.flags = 0, //VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
				.maxSets = 2500,
				.poolSizeCount = (u32)sizes.size(),
				.pPoolSizes = sizes.data(),
			};
			vkCreateDescriptorPool(_up.device, &pool_info, nullptr, &frames[i].descriptor_pool);
			main_delq.push_function([this, i]() {
				vkResetDescriptorPool(_up.device, frames[i].descriptor_pool, 0);
				vkDestroyDescriptorPool(_up.device, frames[i].descriptor_pool, nullptr);
				});
		}
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
		VK_CHECK(vkCreateDescriptorPool(_up.device, &pool_info, nullptr, &imgui_pool));
		ImGui::CreateContext();
		ImGui_ImplGlfw_InitForVulkan(_window.handle, true);
		ImGui_ImplVulkan_InitInfo init_info = {
			.Instance = _vk.vkb_instance.instance,
			.PhysicalDevice = _vk.vkb_device.physical_device.physical_device,
			.Device = _up.device,
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

		ImGui_ImplVulkan_Init(&init_info, _vk.forward_renderpass);
		vku::vk_immediate(_up, [this](VkCommandBuffer cmd) {
			ImGui_ImplVulkan_CreateFontsTexture(cmd);
			});

		ImGui_ImplVulkan_DestroyFontUploadObjects();
		main_delq.push_function([this, imgui_pool]() {
			vkDestroyDescriptorPool(_up.device, imgui_pool, nullptr);
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

	// move outside
	bool zCore::load_shader_module(const char* file_path, VkShaderModule* out_shader) {
		std::filesystem::path file_path_std{ file_path };

		std::ifstream file(file_path, std::ios::ate | std::ios::binary);
		if (!file.is_open()) {
			DBG("couldnt open file: " << std::filesystem::current_path().c_str() << "/" << file_path_std.c_str());
			
			abort();
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
		if (vkCreateShaderModule(_up.device, &create_info, nullptr, &shader_module) != VK_SUCCESS) {
			DBG("Error loading shader: " << file_path);
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

		LocalMesh triangle;
		triangle._vertices.resize(3);
		//vertex positions
		triangle._vertices[0].pos = { 1.f, 1.f, 0.0f };
		triangle._vertices[1].pos = {-1.f, 1.f, 0.0f };
		triangle._vertices[2].pos = { 0.f,-1.f, 0.0f };

		//vertex colors, all green
		triangle._vertices[0].color = { 0.f, 1.f, 0.0f }; //pure green
		triangle._vertices[1].color = { 0.f, 1.f, 0.0f }; //pure green
		triangle._vertices[2].color = { 0.f, 1.f, 0.0f }; //pure green

		auto gpu_triangle = upload_mesh(triangle);

		LocalMesh monkey_mesh;
		monkey_mesh.load_from_obj("../assets/monkey_smooth.obj");
		auto gpu_monkey = upload_mesh(monkey_mesh);



		LocalMesh lost_empire;
		lost_empire.load_from_obj("../assets/lost_empire.obj");
		auto gpu_empire = upload_mesh(lost_empire);

		auto monkey_fkey = render::insert_mesh(assets, gpu_monkey);
		auto triangle_fkey = render::insert_mesh(assets, gpu_triangle);
		auto empire_fkey = render::insert_mesh(assets, gpu_empire);
		render::name_handle(assets, "empire_mesh", empire_fkey);
		render::name_handle(assets, "triangle", triangle_fkey);
		render::name_handle(assets, "monkey", monkey_fkey);
	}

	// NOT OK
	Mesh zCore::upload_mesh(LocalMesh& lmesh) {
		Mesh mesh;
		const size_t buffer_size = lmesh._vertices.size() * sizeof(lmesh._vertices[0]);
		mesh.size = lmesh._vertices.size();

		assert(buffer_size != 0); // you are trying to upload an empty mesh.

		auto staging_buffer = create_buffer(_vk.allocator, buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

		{
			MappedBuffer<char> staging_map{ _vk.allocator, staging_buffer };
			memcpy(staging_map.data, lmesh._vertices.data(), buffer_size);
		}

		if (_vk.allocator->IsIntegratedGpu()) {
			// we dont need to copy, as cpu and gpu visible memory are usually the same
			mesh.vertices = staging_buffer;
		} else {
			// need to copy from cpu to gpu
			mesh.vertices = create_buffer(_vk.allocator, 
				buffer_size, 
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VMA_MEMORY_USAGE_GPU_ONLY);


			// copy from staging to vertex
			vku::vk_immediate(_up, [=](VkCommandBuffer cmd) {
				VkBufferCopy copy;
				copy.dstOffset = 0;
				copy.srcOffset = 0;
				copy.size = buffer_size;
				vkCmdCopyBuffer(cmd, staging_buffer.buffer, mesh.vertices.buffer, 1, &copy);
				});

			vmaDestroyBuffer(_vk.allocator, staging_buffer.buffer, staging_buffer.allocation);
		}
		return mesh;
	}

	void zCore::setup_draw() {
		_df.old_frame_start = std::chrono::steady_clock::now();
		_df.global_current_time = 0.0;
	}

	void zCore::draw() {
		
		auto monitor = glfwGetPrimaryMonitor();
		auto refresh_rate = glfwGetVideoMode(monitor)->refreshRate;
		auto dt = 1.f / 100.f;


		uint32_t swapchain_image_idx;
		auto acquire_result = vkAcquireNextImageKHR(_up.device, _window.swapchain(), 0, current_frame().presentS, nullptr, &swapchain_image_idx);

		render::begin_collect(renderer, _up);
		//for (auto& static_obj : static_renderables) {
		//	render::add_renderable(renderer, static_obj);
		//}

		render::finish_collect(this->renderer);

		if (acquire_result == VK_SUCCESS) {
			// we have an image to render to

			// ------ acquire frame
			auto& frame = current_frame();
			VK_CHECK(vkResetCommandBuffer(frame.buf, 0));
			VkCommandBufferBeginInfo cmd_begin_info = vki::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
			VK_CHECK(vkResetDescriptorPool(_up.device, frame.descriptor_pool, 0));
			VK_CHECK(vkBeginCommandBuffer(frame.buf, &cmd_begin_info));
			// ------ acquire frame end


			// forward pass
			{
				// -- pushing around data into buffers

				glm::mat4 view = _camera.view();
				glm::mat4 projection = _camera.projection();
				GPUCameraData camera_data = {
					.view = view,
					.proj = projection,
					.viewproj = projection * view,
				};

				GPUSceneData scene_data = {
					.camera = camera_data,
				};

				
				render::RenderData rdata = {
					.forward_pass = _vk.forward_renderpass,
					.forward_framebuffer = _vk.forward_framebuffer,
					.forward_extent = _window.extent(),
					.dcache = &_vk.layout_cache,
				};
				VkViewport viewport = vki::viewport_info(_window.extent());
				_camera.aspect = viewport.width / viewport.height;

				render::render(this->renderer, this->assets, frame, this->_up, scene_data, rdata);


				auto imgui_draw_data = ImGui::GetDrawData();
				if (imgui_draw_data != nullptr) {
					ImGui_ImplVulkan_RenderDrawData(imgui_draw_data, current_frame().buf);
				}
				// --

				vkCmdEndRenderPass(current_frame().buf);
			}

			// copy pass here
			{
				VkImageMemoryBarrier depth_image_barrier = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.image = _vk.depth_texture.image,
					.subresourceRange = {
						.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
						.levelCount = VK_REMAINING_MIP_LEVELS,
						.layerCount = VK_REMAINING_ARRAY_LAYERS,
					},

				};

				vkCmdPipelineBarrier(current_frame().buf,
					VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					VK_DEPENDENCY_BY_REGION_BIT,
					0, nullptr,
					0, nullptr,
					1, &depth_image_barrier);

				// -- immediate
				VkDescriptorImageInfo image_buffer_info;
				image_buffer_info.sampler = _vk.default_sampler;
				image_buffer_info.imageView = _vk.screen_texture.view;
				image_buffer_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				VkDescriptorSet copy_set;
				DescriptorBuilder::begin(_up.device, frame.descriptor_pool, _vk.layout_cache)
					.bind_image(0, image_buffer_info, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
					.build(copy_set);

				// -- immediate

				VkExtent2D copy_pass_extent = _window.vkb_swapchain.extent;
				VkRenderPassBeginInfo copy_rp_info = vki::renderpass_begin_info(_vk.copy_pass, _vk.framebuffers[swapchain_image_idx], copy_pass_extent);

				VkViewport viewport = vki::viewport_info(_window.extent());

				VkRect2D scissor = {
					.offset = { 0, 0 },
					.extent = _window.extent(),
				};

				vkCmdSetViewport(frame.buf, 0, 1, &viewport);
				vkCmdSetScissor(frame.buf, 0, 1, &scissor);

				vkCmdBeginRenderPass(frame.buf, &copy_rp_info, VK_SUBPASS_CONTENTS_INLINE);
				auto blit_material = assets.t_materials[assets.t_names["blit"]];

				vkCmdBindPipeline(frame.buf, VK_PIPELINE_BIND_POINT_GRAPHICS, blit_material.pipeline);
				vkCmdBindDescriptorSets(frame.buf, VK_PIPELINE_BIND_POINT_GRAPHICS, blit_material.pipeline_layout, 0, 1, &copy_set, 0, nullptr);

				vkCmdDraw(frame.buf, 3, 1, 0, 0);
				vkCmdEndRenderPass(frame.buf);
			}



			VK_CHECK(vkEndCommandBuffer(current_frame().buf));

			VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			VkSubmitInfo submit_info = {
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.pNext = nullptr,
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &frame.presentS,
				.pWaitDstStageMask = &wait_stage,
				.commandBufferCount = 1,
				.pCommandBuffers = &frame.buf,
				.signalSemaphoreCount = 1,
				.pSignalSemaphores = &frame.renderS,
			};

			vkResetFences(_up.device, 1, &current_frame().renderF);
			VK_CHECK(vkQueueSubmit(_vk.graphics_queue, 1, &submit_info, frame.renderF));
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

			if (vkGetFenceStatus(_up.device, current_frame().renderF) == VK_SUCCESS) 	{ // actual rendering
				auto start_frame = std::chrono::steady_clock::now();
				auto anim_dt = start_frame - _df.old_frame_start;
				auto anim_dt_float = std::chrono::duration<float>(anim_dt).count();
				frame_times.push_back(anim_dt_float);
				_df.global_current_time += anim_dt_float;

				// -- imgui
				ImGui_ImplVulkan_NewFrame();
				ImGui_ImplGlfw_NewFrame();
				ImGui::NewFrame();
				ImGui::Begin("Debug tool", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
				ImGui::PlotLines("", frame_times.linearize(), (i32)frame_times.size(), 0, "Frame DT", 0.f, 0.1f, ImVec2(250, 100), 4);
				auto avg_ft = std::accumulate(frame_times.begin(), frame_times.end(), 0.f)/frame_times.size();
				
				ImGui::Text("_gt %f", (float)_df.global_current_time);
				ImGui::Text("_frameidx %u", current_frame_idx());
				ImGui::Text("avg frametime %f", avg_ft);
				ImGui::Text("Number of objects: %u", renderer.t_statics.size() + renderer.t_objects.size());
				ImGui::SliderFloat("Horizontal speed", &speed, 1.f, 50.f);
				ImGui::SliderFloat("Vertical speed", &fly_speed, 1.f, 50.f);

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
		load_image_from_file(_up, "../assets/lost_empire-RGBA.png", lost_empire.image, lost_empire.allocation);
		VkImageViewCreateInfo image_info = vki::imageview_create_info(VK_FORMAT_R8G8B8A8_SRGB, lost_empire.image,  VK_IMAGE_ASPECT_COLOR_BIT);
		vkCreateImageView(_up.device, &image_info, nullptr, &lost_empire.view);
		lost_empire.format = image_info.format;

		auto empire_handle = render::insert_texture(assets, lost_empire);
		render::name_handle(assets, "empire_diffuse", empire_handle);
	}
}