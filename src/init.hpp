#pragma once
#include "acceleration_structures.hpp"
#include "camera.hpp"
#include "descriptors.hpp"
#include "lights.hpp"
#include "loader.hpp"
#include "presampling.hpp"
#include "rt_pipelines.hpp"
#include "shader_binding_tables.hpp"
#include "types.hpp"
#include <SDL3/SDL.h>
#include <memory>
#include <queue>
#include <vk_mem_alloc.h>

class Init
{
public:
    // Initializes everything in the engine
    Init(const std::filesystem::path &gltfPath);
    ~Init() = default;

    // Shuts down the engine
    void clean();

    // Public init calls
    void recreate_swapchain();
    void recreate_camera();
    void recreate_draw_data();

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
    std::vector<ImageData> swapchainImages;

    // Camera
    Camera camera{};

    // Вescriptors
    std::unique_ptr<DescHelper> descHelperUAB, descHelperRt;
    vk::DescriptorSetLayout descriptorSetLayoutUAB;
    vk::DescriptorSetLayout rtDescriptorSetLayout;

    // Pipelines
    SimplePipelineData simpleRtPipeline;
    std::unique_ptr<RtPipelineBuilder> rtPipelineBuilder;

    // Envmap
    ImageData backgroundImage;

    // Imgui
    vk::DescriptorPool imguiPool;

    // Ray tracing
    vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{};
    vk::PhysicalDeviceAccelerationStructurePropertiesKHR asProperties{};
    TopLevelAS tlas;
    Buffer rtSBTBuffer;
    std::unique_ptr<SbtHelper> sbtHelper;
    std::unique_ptr<ASBuilder> asBuilder;
    std::unique_ptr<Presampler> presampler;

    // Meshes
    std::unique_ptr<GLTFLoader> gltfLoader;
    std::shared_ptr<GLTFObj> scene;

    bool isInitialized{false};

    void rebuid_rt_pipeline(const SpecializationConstantsClosestHit &constantsCH,
                            const SpecializationConstantsMiss &constantsMiss);

    void load_background(const std::filesystem::path &imPath = std::filesystem::path(
                             std::string(PROJECT_DIR)
                             + std::string("/assets/rogland_clear_night_4k.hdr")));

private:
    // Initialization calls
    void init_sdl();
    void init_vulkan();
    void init_rt();
    void presample();
    void init_commands();
    void init_sync_structures();
    void init_descriptors();
    void init_pipelines();
    void create_sbt();
    void init_imgui();
    void load_meshes(const std::filesystem::path &gltfPath);
    void create_as();

    std::queue<vk::Pipeline> rtPipelineQueue;
    std::queue<Buffer> rtSBTBufferQueue;
};
