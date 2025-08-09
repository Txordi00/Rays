#pragma once
#include "descriptors.hpp"
#include "model.hpp"
#include "raster_pipelines.hpp"
#include "types.hpp"
#include "uniform_buffers.hpp"
#include <SDL3/SDL.h>
#include <memory>
#include <vk_mem_alloc.h>

class Init
{
public:
    // Initializes everything in the engine
    Init();
    ~Init() = default;

    // Shuts down the engine
    void clean();

    // First init() calls
    void create_swapchain(uint32_t width, uint32_t height);

    SDL_Window *window{nullptr};
    // Strucutres gotten at init time
    vk::Instance instance;
    vk::DebugUtilsMessengerEXT debugMessenger;
    vk::PhysicalDevice physicalDevice;
    vk::Device device;
    vk::SurfaceKHR surface;
    VmaAllocator allocator;
    vk::PhysicalDeviceProperties physicalDeviceProperties;

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

    // Swapchain structures and functions
    vk::SwapchainKHR swapchain;
    vk::Format swapchainImageFormat;
    vk::Extent2D swapchainExtent;
    std::vector<vk::Image> swapchainImages;
    std::vector<vk::ImageView> swapchainImageViews;

    // Draw date
    ImageData imageDraw;
    ImageData imageDepth;
    // vk::Extent2D imageDrawExtent;

    // Descriptors data and functions
    std::unique_ptr<DescriptorPool> descriptorPool;
    vk::DescriptorSet drawImageDescriptors;
    DescriptorSetData drawImageDescriptorsData;

    // Uniform buffers
    std::unique_ptr<Ubo> ubo;
    vk::DescriptorSetLayout uboDescriptorSetLayout;
    std::vector<vk::DescriptorSet> uboDescriptorSets;
    std::vector<Buffer> uniformBuffers;

    // Pipelines
    std::vector<ComputePipelineData> computePipelines;
    SimplePipelineData simpleMeshGraphicsPipeline;

    // Imgui
    vk::DescriptorPool imguiPool;

    // Ray tracing
    vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{};
    vk::PhysicalDeviceAccelerationStructurePropertiesKHR asProperties{};

    // Meshes
    std::vector<std::shared_ptr<Model>> models;

    bool isInitialized{false};

private:
    // Initialization calls
    void init_vulkan();
    void create_draw_data();
    void init_commands();
    void init_sync_structures();
    void init_compute_descriptors();
    void init_ub_descriptors();
    void init_pipelines();
    void init_imgui();
    void load_meshes();
    void init_rt();

    void destroy_swapchain();
};
