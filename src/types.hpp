#pragma once
#ifndef USE_CXX20_MODULES
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif
#include <iostream>

#define VK_CHECK(x) \
    { \
        vk::Result res = static_cast<vk::Result>(x); \
        if (res != vk::Result::eSuccess) { \
            std::cerr << "\033[1;33m" << "NON SUCCESSFUL vk::Result: " << vk::to_string(res) \
                      << "\033[0m" << std::endl; \
        } \
    }

// IT DOES NOT WORK YET
#define VK_CHECK_E(x) \
    { \
        try { \
            x; \
        } catch (const std::exception &e) { \
            std::cerr << "\033[1;33m" << "Vulkan exception: " << e.what() << "\033[0m" \
                      << std::endl; \
        } \
    }

std::exception e;

const unsigned int W = 1000;
const unsigned int H = 1000;
const std::string PROJNAME = "LRT";
const unsigned int API_VERSION[3] = {1, 3, 0};

const vk::PresentModeKHR PRESENT_MODE = vk::PresentModeKHR::eFifoRelaxed;
const unsigned int MINIMUM_FRAME_OVERLAP = 3;
const uint64_t FENCE_TIMEOUT = 1000000000;

const std::string GRADIENT_COMP_SHADER_FP
    = "/home/jordi/Documents/lrt/build/clang_ninja-Debug/shaders/gradient.comp.spv";

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif
