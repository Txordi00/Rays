cmake_minimum_required(VERSION 3.25)
project(lrt LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The user can set this to ON or OFF
set(USE_VULKANHPP_CPP20_MODULE OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
add_compile_options(-Wno-nullability-completeness -Wno-deprecated-literal-operator)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -ffast-math")

find_package(Vulkan REQUIRED)

# Require Vulkan version ≥ 1.3.256 (earliest version where the Vulkan CXX20 module was available)
if( ${Vulkan_VERSION} VERSION_LESS "1.3.256" )
    message( FATAL_ERROR "Minimum supported Vulkan version is 1.3.256. "
           "Found ${Vulkan_VERSION}."
  )
endif()

# set up Vulkan C++ module as a library
if(USE_VULKANHPP_CPP20_MODULE)
    add_library(VulkanHppModule)
    target_sources( VulkanHppModule PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS ${Vulkan_INCLUDE_DIR}
    FILES ${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm)
    target_compile_features(VulkanHppModule PUBLIC cxx_std_23)
    target_compile_definitions(VulkanHppModule PUBLIC VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)
    target_link_libraries(VulkanHppModule PUBLIC Vulkan::Headers)
endif()

# GLM
find_package(glm CONFIG REQUIRED)
# SDL3
find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
# ImGui
add_library(imgui STATIC "include/imgui/imgui.cpp" "include/imgui/imgui_demo.cpp"
  "include/imgui/imgui_draw.cpp" "include/imgui/imgui_tables.cpp" "include/imgui/imgui_widgets.cpp"
  "include/imgui/imgui_impl_sdl3.cpp" "include/imgui/imgui_impl_vulkan.cpp")
target_link_libraries(imgui PRIVATE Vulkan::Vulkan)
# Fastgltf
set(FASTGLTF_COMPILE_AS_CPP20 YES)
add_subdirectory("third_party_non_include/fastgltf")

# Vk-bootstrap
include(FetchContent)
FetchContent_Declare(
    fetch_vk_bootstrap
    GIT_REPOSITORY "https://github.com/charles-lunarg/vk-bootstrap"
    GIT_TAG        "v1.4.328" #suggest using a tag so the library doesn't update whenever new commits are pushed to a branch
)
FetchContent_MakeAvailable(fetch_vk_bootstrap)

# Add all folders with code (with or without their own CMakeLists.txt)
add_executable(${PROJECT_NAME} "src/main.cpp")
add_subdirectory("src")
add_subdirectory("shaders")
target_include_directories(${PROJECT_NAME} PRIVATE "include")

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)
target_link_libraries(${PROJECT_NAME} PRIVATE vk-bootstrap::vk-bootstrap)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui)
target_link_libraries(${PROJECT_NAME} PRIVATE fastgltf::fastgltf)

# Link either vulkan either the vulkan_hpp module
if(USE_VULKANHPP_CPP20_MODULE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_CXX20_MODULES)
    target_link_libraries(${PROJECT_NAME} PRIVATE VulkanHppModule)
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)
    target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Headers)
    target_precompile_headers(${PROJECT_NAME} PRIVATE <vulkan/vulkan.hpp> <vulkan/vulkan_handles.hpp> <vulkan/vulkan_structs.hpp> <vulkan/vulkan_funcs.hpp>)
endif()

# Compile the shaders on build
add_dependencies(${PROJECT_NAME} shaders)
