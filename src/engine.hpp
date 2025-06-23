#pragma once

// #ifndef USE_CXX20_MODULES
// #else
// import vulkan_hpp;
// #endif
#include "descriptors.hpp"
#include "loader.hpp"
#include "raster_pipelines.hpp"
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <vk_mem_alloc.h>

class Engine
{
public:
    Engine();
    ~Engine();

    // draw loop
    void draw();

    // run main loop
    void run();

    // Return frame
    FrameData &get_current_frame() { return frames[frameNumber % frameOverlap]; }

private:
    // initializes everything in the engine
    void init();

    // shuts down the engine
    void clean();

    // First init() calls
    void init_vulkan();
    void create_draw_data();
    void init_commands();
    void init_sync_structures();
    void load_meshes();

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
    vk::Queue transferQueue;
    uint32_t transferQueueFamilyIndex;
    vk::Fence transferFence;
    vk::CommandPool transferCmdPool;
    vk::CommandBuffer cmdTransfer;

    // Descriptors data and functions
    std::unique_ptr<DescriptorPool> descriptorPool;
    vk::DescriptorSet drawImageDescriptors;
    DescriptorSetData drawImageDescriptorsData;
    void init_descriptors();

    // Pipelines
    void init_pipelines();
    std::vector<ComputePipelineData> computePipelines;
    int currentBackgroundPipelineIndex{1};
    SimplePipelineData simpleMeshGraphicsPipeline;

    // Meshes
    std::vector<std::shared_ptr<DeviceMeshAsset>> gpuMeshes;

    // Draw commands
    void change_background(const vk::CommandBuffer &cmd);
    void draw_meshes(const vk::CommandBuffer &cmd);

    // Imgui
    vk::DescriptorPool imguiPool;
    void init_imgui();
    void draw_imgui(const vk::CommandBuffer &cmd, const vk::ImageView &imageView);

    // Buffers
    Buffer create_buffer(const vk::DeviceSize &size,
                         const vk::BufferUsageFlags &usageFlags,
                         const VmaMemoryUsage &memoryUsage,
                         const VmaAllocationCreateFlags &allocationFlags);
    void destroy_buffer(const Buffer &buffer);
    MeshBuffer create_mesh(const std::span<uint32_t> &indices, const std::span<Vertex> &vertices);

    // Other data
    bool isInitialized{false};
    uint64_t frameNumber{0};
    bool stopRendering{false};
    SDL_Window *window{nullptr};
};
