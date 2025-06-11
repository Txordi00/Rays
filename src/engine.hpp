#pragma once

// #ifndef USE_CXX20_MODULES
// #else
// import vulkan_hpp;
// #endif
#include "descriptors.hpp"
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <vk_mem_alloc.h>

struct FrameData
{
    vk::CommandPool commandPool;
    vk::CommandBuffer mainCommandBuffer;
    vk::Semaphore swapchainSemaphore, renderSemaphore;
    vk::Fence renderFence;
};

struct ImageData
{
    vk::Image image;
    vk::ImageView imageView;
    VmaAllocation allocation;
    vk::Extent3D extent;
    vk::Format format;
};

struct GradientColorPush
{
    glm::vec4 colorUp;
    glm::vec4 colorDown;
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
    FrameData &get_current_frame() { return frames[frameNumber % frameOverlap]; }

private:
    //initializes everything in the engine
    void init();

    //shuts down the engine
    void clean();

    // Functions that init() calls
    void init_vulkan();
    void create_draw_data();
    void init_commands();
    void init_sync_structures();
    void init_imgui();

    // A command that changes the color of the background
    void change_background(vk::CommandBuffer &cmd);

    // Strucutres gotten at init time
    vk::Instance instance;
    vk::DebugUtilsMessengerEXT debugMessenger;
    vk::PhysicalDevice physicalDevice;
    vk::Device device;
    vk::SurfaceKHR surface;
    VmaAllocator allocator;
    vk::PhysicalDeviceProperties physicalDeviceProperties;

    // Swapchain structures and functions
    vk::SwapchainKHR swapchain;
    vk::Format swapchainImageFormat;
    vk::Extent2D swapchainExtent;
    std::vector<vk::Image> swapchainImages;
    std::vector<vk::ImageView> swapchainImageViews;
    void create_swapchain(uint32_t width, uint32_t height);
    void destroy_swapchain();

    // Draw date
    ImageData imageDraw;
    // vk::Extent2D imageDrawExtent;

    // Commands data
    std::vector<FrameData> frames;
    unsigned int frameOverlap;
    vk::Queue graphicsQueue;
    uint32_t graphicsQueueFamilyIndex;

    // Descriptors data and functions
    std::unique_ptr<DescriptorPool> descriptorPool;
    vk::DescriptorSet drawImageDescriptors;
    DescriptorSetData drawImageDescriptorsData;
    void init_descriptors();

    // Pipelines
    void init_pipelines();
    void init_background_compute_pipeline();
    vk::Pipeline backgroundComputePipeline;
    vk::PipelineLayout backgroundComputePipelineLayout;

    // Imgui data
    vk::DescriptorPool imguiPool;
    void draw_imgui(const vk::CommandBuffer &cmd, const vk::ImageView &imageView);

    // Other data
    bool isInitialized{false};
    uint64_t frameNumber{0};
    bool stopRendering{false};
    SDL_Window *window{nullptr};
};
