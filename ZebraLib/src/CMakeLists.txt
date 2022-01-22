# CMakeList.txt : CMake project for ZebraLib, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)


# Add source to this project's executable.
add_executable (zebralib
	"main.cpp" 
	"vki.cpp"
	"vki.h"
	"zebralib.h"
	"zebralib.cpp"
	"zebratypes.h"
 "g_pipeline.h"
 "g_pipeline.cpp" 
 "g_types.h" 
 "g_mesh.h"
 "g_mesh.cpp" 
 "g_camera.h"
 "g_camera.cpp"  "g_vec.h" "z_debug.h" "g_texture.h"
 "g_texture.cpp" "g_buffer.h" "g_buffer.cpp" "g_descriptorset.h" "g_descriptorset.cpp" "g_vku.h" "g_vku.cpp" "renderer.h" "d_rel.h" "d_rel.cpp" "renderer.cpp")

if (CMAKE_COMPILER_IS_GNUCC )
 target_compile_options(zebralib PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
endif()

 
set_property(TARGET zebralib PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:zebralib")

target_include_directories(zebralib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(zebralib vkbootstrap vma glm tinyobjloader imgui stb_image magic_enum boost_headers)
target_link_libraries(zebralib Vulkan::Vulkan glfw)

add_dependencies(zebralib Shaders)