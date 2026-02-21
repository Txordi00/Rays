cmake_minimum_required(VERSION 3.25)
# The user can set this to ON or OFF
# WARING: ON is very experimental, and it currently breaks everywhere due to the forced import
# of the std module in the vulkan module
set(USE_VULKANHPP_CPP20_MODULE OFF)

set(CMAKE_CXX_STANDARD 23)
if(USE_VULKANHPP_CPP20_MODULE)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_SCAN_FOR_MODULES ON)
  set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
  set(CMAKE_CXX_MODULE_STD ON)
endif()
add_compile_options(-Wno-nullability-completeness -Wno-deprecated-literal-operator)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -ffast-math")

# set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(rays LANGUAGES CXX)


find_package(Vulkan REQUIRED)

# Require Vulkan from the system at version ≥ 1.3.256 (earliest version where the Vulkan CXX20 module was available)
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
    FILES
    ${Vulkan_INCLUDE_DIR}/vulkan/vulkan_video.cppm # Need to rename vulkan_hpp:video -> vulkan:video in this file
    ${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm)
    target_compile_features(VulkanHppModule PUBLIC cxx_std_23)
    target_compile_definitions(VulkanHppModule PUBLIC VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1 VULKAN_HPP_NO_STD_MODULE=1)
    target_link_libraries(VulkanHppModule PUBLIC Vulkan::Headers)
endif()

# GLM from system
find_package(glm CONFIG REQUIRED)
# SDL3 from system
find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)

# ImGui
include(FetchContent)
FetchContent_Declare(
  imgui
  GIT_REPOSITORY  "https://github.com/ocornut/imgui.git"
  GIT_TAG         "v1.92.6"
)
FetchContent_MakeAvailable(imgui)

# Create the imgui library target
add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

# Add include directories
target_include_directories(imgui PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui PRIVATE Vulkan::Vulkan SDL3::SDL3)


# Vk-bootstrap
FetchContent_Declare(
    fetch_vk_bootstrap
    GIT_REPOSITORY "https://github.com/charles-lunarg/vk-bootstrap"
    GIT_TAG        "v${Vulkan_VERSION}"
)
FetchContent_MakeAvailable(fetch_vk_bootstrap)


# Fastgltf
FetchContent_Declare(
  fastgltf
  GIT_REPOSITORY  "https://github.com/spnda/fastgltf.git"
  GIT_TAG         "main"
)
set(FASTGLTF_COMPILE_AS_CPP20 YES)
FetchContent_MakeAvailable(fastgltf)

# VMA
FetchContent_Declare(
  vma
  GIT_REPOSITORY  "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git"
  GIT_TAG         "master"
)
FetchContent_MakeAvailable(vma)

# stb_image
FetchContent_Declare(
  stb
  GIT_REPOSITORY  "https://github.com/nothings/stb.git"
  GIT_TAG         "master"
)
FetchContent_MakeAvailable(stb)

# nativefiledialog-extended
FetchContent_Declare(
    nfd
    GIT_REPOSITORY "https://github.com/btzy/nativefiledialog-extended.git"
    GIT_TAG "master"
)
FetchContent_MakeAvailable(nfd)


# Add all folders with code (with or without their own CMakeLists.txt)
add_executable(${PROJECT_NAME} "src/main.cpp")
add_subdirectory("src")
add_subdirectory("shaders")
target_include_directories(${PROJECT_NAME} PRIVATE ${stb_SOURCE_DIR} ${vma_SOURCE_DIR}/include)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)
target_link_libraries(${PROJECT_NAME} PRIVATE vk-bootstrap::vk-bootstrap)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui)
target_link_libraries(${PROJECT_NAME} PRIVATE fastgltf::fastgltf)
target_link_libraries(${PROJECT_NAME} PRIVATE nfd)

# Compile definitions to be used from the C++ source files
target_compile_definitions(${PROJECT_NAME} PRIVATE PROJECT_DIR="${PROJECT_SOURCE_DIR}")


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
