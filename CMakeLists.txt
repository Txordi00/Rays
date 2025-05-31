cmake_minimum_required(VERSION 3.25)

project(lrt LANGUAGES CXX)
add_compile_options(-Wno-nullability-completeness)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

find_package(Vulkan REQUIRED)

# Require Vulkan version ≥ 1.3.256 (earliest version when the Vulkan module was available)
if( ${Vulkan_VERSION} VERSION_LESS "1.3.256" )
  message( FATAL_ERROR "Minimum supported Vulkan version is 1.3.256. "
           "Found ${Vulkan_VERSION}."
  )
endif()

# set up Vulkan C++ module as a library
add_library(VulkanHppModule)
target_sources( VulkanHppModule PUBLIC
  FILE_SET CXX_MODULES
  BASE_DIRS ${Vulkan_INCLUDE_DIR}
  FILES ${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm)
target_compile_features(VulkanHppModule PUBLIC cxx_std_23)
target_compile_definitions(VulkanHppModule PUBLIC VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)
target_link_libraries(VulkanHppModule PUBLIC Vulkan::Headers)



# GLM
find_package(glm CONFIG REQUIRED)
# SDL3
find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)

# Vk-bootstrap
include(FetchContent)
FetchContent_Declare(
    fetch_vk_bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
    GIT_TAG        main #suggest using a tag so the library doesn't update whenever new commits are pushed to a branch
)
FetchContent_MakeAvailable(fetch_vk_bootstrap)

add_executable(${PROJECT_NAME} "src/main.cpp")
add_subdirectory("src")
# add_subdirectory("shaders")
target_include_directories(${PROJECT_NAME} PRIVATE "include")

target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)
# target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)
target_link_libraries(${PROJECT_NAME} PRIVATE VulkanHppModule)
target_link_libraries(${PROJECT_NAME} PRIVATE vk-bootstrap::vk-bootstrap)
# target_compile_definitions(${PROJECT_NAME} PRIVATE USE_CXX20_MODULES)
target_precompile_headers(${PROJECT_NAME} PRIVATE <vulkan/vulkan.hpp> <vulkan/vulkan_handles.hpp> <vulkan/vulkan_structs.hpp> <vulkan/vulkan_funcs.hpp>)

# add_dependencies(${PROJECT_NAME} shaders)
