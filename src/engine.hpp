#pragma once

// #ifndef USE_CXX20_MODULES
// #else
// import vulkan_hpp;
// #endif
#include "camera.hpp"
#include "init.hpp"
#include <memory>

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
    FrameData &get_current_frame() { return I->frames[frameNumber]; }

private:
    // initializes everything in the engine
    std::unique_ptr<Init> I;

    // Camera
    Camera camera{};

    // Draw commands
    // void change_background(const vk::CommandBuffer &cmd);
    void draw_meshes(const vk::CommandBuffer &cmd);

    // Imgui
    void draw_imgui(const vk::CommandBuffer &cmd, const vk::ImageView &imageView);

    // Buffers
    // void destroy_buffer(const Buffer &buffer);
    // MeshBuffer create_mesh(const std::span<uint32_t> &indices, const std::span<Vertex> &vertices);

    // Other data
    uint64_t frameNumber{0};
    uint32_t swapchainImageIndex{0};
    bool stopRendering{false};
    // int currentBackgroundPipelineIndex{1};
};
