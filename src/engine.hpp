#pragma once

// #ifndef USE_CXX20_MODULES
// #else
// import vulkan_hpp;
// #endif
#include "init.hpp"
#include <memory>

class Engine
{
public:
    Engine();
    ~Engine();

    // run main loop
    void run();


private:
    // initializes everything in the engine
    std::unique_ptr<Init> I;

    // Return frame
    FrameData &get_current_frame() { return I->frames[frameNumber]; }

    void update_imgui();

    // Inform to the shaders about the resources
    std::unique_ptr<DescriptorUpdater> descUpdater;
    void update_descriptors();

    // draw loop
    void draw();

    // record main command buffer
    void record_frame_cmds();

    // Draw commands
    void raster(const vk::CommandBuffer &cmd);

    // Ray tracing commands
    void raytrace(const vk::CommandBuffer &cmd);

    // Imgui
    void draw_imgui(const vk::CommandBuffer &cmd, const vk::ImageView &imageView);

    // Other data
    uint64_t frameNumber{0};
    uint32_t swapchainImageIndex{0};
    bool stopRendering{false};

    // RT push constants
    RayPush rayPush{};

    // Lights manager
    std::unique_ptr<LightsManager> lightsManager;
};
