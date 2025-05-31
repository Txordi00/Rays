#pragma once
#include <iostream>
#ifndef USE_CXX20_MODULES
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#include <iostream>
#endif

#define VK_CHECK(x) \
    { \
        vk::Result res = static_cast<vk::Result>(x); \
        if (res != vk::Result::eSuccess) \
            std::cerr << "\033[1;33m" << "NO SUCCESSFUL vk::Result: " << vk::to_string(res) \
                      << "\033[0m" << std::endl; \
    }

const unsigned int W = 1000;
const unsigned int H = 1000;
const std::string PROJNAME = "LRT";
const unsigned int API_VERSION[3] = {1, 3, 0};

const vk::PresentModeKHR PRESENT_MODE = vk::PresentModeKHR::eFifoRelaxed;
const unsigned int FRAME_OVERLAP = 2;
const uint64_t FENCE_TIMEOUT = 1000000000;

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif
