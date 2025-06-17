#pragma once
#ifndef USE_CXX20_MODULES
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif
#include <exception>
#include <iostream>

#define VK_CHECK_RES(x) \
    { \
        vk::Result res = static_cast<vk::Result>(x); \
        if (res != vk::Result::eSuccess) { \
            std::cerr << "\033[1;33m" << "NON SUCCESSFUL vk::Result: " << vk::to_string(res) \
                      << "\033[0m" << std::endl; \
        } \
    }

// To be used inside the catch part of a try and catch statement
#define VK_CHECK_EXC(exception) \
    { \
        std::string err_str = std::string("\033[1;33m + Vulkan exception: ") + exception.what() \
                              + std::string("\033[0m\n"); \
        throw std::runtime_error(err_str); \
    }

// #define VK_CHECK_E(x) \
//     { \
//         try { \
//             x; \
//         } catch (const std::exception &e) { \
//             std::cerr << "\033[1;33m" << "Vulkan exception: " << e.what() << "\033[0m" \
//                       << std::endl; \
//         } \
//     }

const unsigned int W = 1000;
const unsigned int H = 1000;
const std::string PROJNAME = "LRT";
const unsigned int API_VERSION[3] = {1, 3, 0};

const vk::PresentModeKHR PRESENT_MODE = vk::PresentModeKHR::eFifoRelaxed;
const unsigned int MINIMUM_FRAME_OVERLAP = 3;
const uint64_t FENCE_TIMEOUT = 1000000000;

const std::string GRADIENT_COMP_SHADER_FP = "shaders/gradient.comp.spv";
const std::string GRADIENT_COLOR_COMP_SHADER_FP = "shaders/gradient_color.comp.spv";
const std::string SKY_SHADER_FP = "shaders/sky.comp.spv";
#define TRIANGLE_VERT_SHADER "shaders/triangle.vert.spv"
#define TRIANGLE_FRAG_SHADER "shaders/triangle.frag.spv"
#define TRIANGLE_MESH_VERT_SHADER "shaders/triangle_mesh.vert.spv"

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

