# Libraries
set(VKP_LIBRARIES_ROOT "${CMAKE_SOURCE_DIR}/Vulkan/vendor")

# Source files
file(GLOB_RECURSE VKP_HEADERS src/*.hpp)
file(GLOB_RECURSE VKP_SOURCES src/*.cpp)

add_executable(Vulkan ${VKP_HEADERS} ${VKP_SOURCES})
target_precompile_headers(Vulkan PRIVATE ${CMAKE_SOURCE_DIR}/Vulkan/src/Pch.hpp)
target_include_directories(Vulkan PRIVATE src)

# Rendering back-end
find_package(Vulkan REQUIRED)
target_link_libraries(Vulkan PRIVATE ${Vulkan_LIBRARY})
target_include_directories(Vulkan PRIVATE ${Vulkan_INCLUDE_DIR})

# SDL
find_package(SDL2 REQUIRED)
target_link_libraries(Vulkan PRIVATE ${SDL2_LIBRARIES})
target_include_directories(Vulkan PRIVATE ${SDL2_INCLUDE_DIRS})

# GLM
set(GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)

add_subdirectory(${VKP_LIBRARIES_ROOT}/glm)
target_include_directories(Vulkan PRIVATE ${VKP_LIBRARIES_ROOT}/glm)
target_link_libraries(Vulkan PRIVATE glm)

# spdlog
set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTING OFF CACHE BOOL "" FORCE)

target_include_directories(Vulkan PRIVATE ${VKP_LIBRARIES_ROOT}/spdlog/include)
add_subdirectory(${VKP_LIBRARIES_ROOT}/spdlog)
target_link_libraries(Vulkan PRIVATE spdlog)

# shaderc
# set(SHADERC_SKIP_INSTALL OFF CACHE BOOL "" FORCE)
# set(SHADERC_SKIP_TESTS ON CACHE BOOL "" FORCE)
# set(SHADERC_SKIP_EXAMPLES ON CACHE BOOL "" FORCE)

# if(MSVC)
# 	set(SHADERC_ENABLE_SHARED_CRT OFF CACHE BOOL "" FORCE)
# endif()

# set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "" FORCE)
# set(SKIP_SPIRV_TOOLS_INSTALL ON CACHE BOOL "" FORCE)
# set(SKIP_GOOGLETEST_INSTALL ON CACHE BOOL "" FORCE)
# set(SPIRV_HEADERS_SKIP_EXAMPLES ON CACHE BOOL "" FORCE)
# set(SPIRV_HEADERS_SKIP_INSTALL ON CACHE BOOL "" FORCE)
# set(SKIP_SPIRV_TOOLS_INSTALL ON CACHE BOOL "" FORCE)
# set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "" FORCE)
# set(SPIRV_SKIP_TESTS ON CACHE BOOL "" FORCE)
# set(RE2_BUILD_TESTING OFF CACHE BOOL "" FORCE)
# set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "" FORCE)
# set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
# set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
# set(ENABLE_CTEST OFF CACHE BOOL "" FORCE)

# target_include_directories(Vulkan PRIVATE ${VKP_LIBRARIES_ROOT}/shaderc/libshaderc/include)
# add_subdirectory(${VKP_LIBRARIES_ROOT}/shaderc)
# target_link_libraries(Vulkan shaderc)

# Vulkan Memory Allocator
set(VMA_BUILD_SAMPLE OFF CACHE BOOL "" FORCE)
set(VMA_BUILD_SAMPLE_SHADERS OFF CACHE BOOL "" FORCE)
set(VMA_BUILD_REPLAY OFF CACHE BOOL "" FORCE)

add_subdirectory(${VKP_LIBRARIES_ROOT}/VulkanMemoryAllocator)
target_include_directories(Vulkan PRIVATE ${VKP_LIBRARIES_ROOT}/VulkanMemoryAllocator/include)
target_link_libraries(Vulkan PRIVATE VulkanMemoryAllocator)