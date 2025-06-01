#pragma once

// #ifndef USE_CXX20_MODULES
// #else
// import vulkan_hpp;
// #endif
#include "types.hpp"
#include <SDL3/SDL.h>
#include <vector>

struct FrameData
{
    vk::CommandPool commandPool;
    vk::CommandBuffer mainCommandBuffer;
    vk::Semaphore swapchainSemaphore, renderSemaphore;
    vk::Fence renderFence;
};

class Engine
{
public:
    Engine();

    ~Engine();

    //draw loop
    void draw();

    //run main loop
    void run();

    // Return frame
    FrameData &get_current_frame() { return frames[frameNumber % FRAME_OVERLAP]; };

private:
    //initializes everything in the engine
    void init();

    //shuts down the engine
    void clean();

    void init_vulkan();
    void init_swapchain();
    void init_commands();
    void init_sync_structures();

    // Strucutres gotten at init time
    vk::Instance instance;
    vk::DebugUtilsMessengerEXT debugMessenger;
    vk::PhysicalDevice physicalDevice;
    vk::Device device;
    vk::SurfaceKHR surface;

    // Swapchain structures and functions
    vk::SwapchainKHR swapchain;
    vk::Format swapchainImageFormat;
    vk::Extent2D windowExtent;
    std::vector<vk::Image> swapchainImages;
    std::vector<vk::ImageView> swapchainImageViews;
    void create_swapchain(uint32_t width, uint32_t height);
    void destroy_swapchain();

    // Commands data
    FrameData frames[FRAME_OVERLAP];
    vk::Queue graphicsQueue;
    uint32_t graphicsQueueFamilyIndex;

    bool isInitialized{false};
    uint64_t frameNumber{0};
    bool stopRendering{false};
    SDL_Window *window{nullptr};
};
