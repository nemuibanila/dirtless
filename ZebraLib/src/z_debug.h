#pragma once
#include <iostream>
#include <vulkan/vulkan.h>

#define DBG(x) std::cerr<<"["<<__func__<<"]"<<x<<std::endl;

#define VK_CHECK(x) do {\
VkResult err = x;\
if (err) { std::cerr << "[" << __func__  << '|' << __FILE__ << ':' << __LINE__ << "]" << "vulkan error: " << err << std::endl; abort();}\
} while (false)

;
