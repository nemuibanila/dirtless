# CMakeList.txt : CMake project for ZebraLib, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
add_executable (zebralib
	main.cc 
	vki.cc
	vki.h
	zebralib.h
	zebralib.cc
	zebratypes.h
 "g_pipeline.h"
 "g_pipeline.cc" 
 "g_types.h" 
 "g_mesh.h"
 "g_mesh.cc" 
 "g_camera.h"
 "g_camera.cc"  "g_vec.h" "z_debug.h")

set_property(TARGET zebralib PROPERTY VS_DEBUFGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:zebralib")

target_include_directories(zebralib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(zebralib vkbootstrap vma glm tinyobjloader imgui stb_image magic_enum boost_headers)
target_link_libraries(zebralib Vulkan::Vulkan glfw)

add_dependencies(zebralib Shaders)