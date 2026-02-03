#pragma once
#ifndef USE_CXX20_MODULES
#include <vulkan/vulkan.hpp>
#else
import vulkan;
#endif

#include "init.hpp"
#include <memory>

class Engine
{
public:
    Engine(const std::filesystem::path &gltfPath);
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

    // Deprecated for now
    // void raster(const vk::CommandBuffer &cmd);

    // Ray tracing commands
    void raytrace(const vk::CommandBuffer &cmd);

    // Imgui
    void draw_imgui(const vk::CommandBuffer &cmd, const vk::ImageView &imageView);

    // Resize
    void resize();

    // Other data
    uint64_t frameNumber{0};
    uint32_t swapchainImageIndex{0};
    bool stopRendering{false};
    bool shouldResize{false};

    // RT push constants
    RayPush rayPush{};

    // Lights manager
    std::unique_ptr<LightsManager> lightsManager;
};
