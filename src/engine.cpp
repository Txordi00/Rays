#ifndef USE_CXX20_MODULES
#include "engine.hpp"
#include "utils.hpp"
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <cmath>
#include <thread>
#else
import vulkan_hpp;
#include "engine.hpp"
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <cmath>
#include <thread>
#include <vulkan/vulkan_hpp_macros.hpp>
#endif

Engine::Engine()
{
    init();
}

void Engine::init()
{
    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    window = SDL_CreateWindow(PROJNAME.c_str(), W, H, SDL_WINDOW_VULKAN);

    init_vulkan();
    init_swapchain();
    init_commands();
    init_sync_structures();

    isInitialized = true;
}
Engine::~Engine()
{
    clean();
}

void Engine::clean()
{
    if (isInitialized) {
        device.waitIdle();
        for (int i = 0; i < FRAME_OVERLAP; i++) {
            device.destroyCommandPool(frames[i].commandPool);
            device.destroyFence(frames[i].renderFence);
            device.destroySemaphore(frames[i].renderSemaphore);
            device.destroySemaphore(frames[i].swapchainSemaphore);
        }
        destroy_swapchain();
        instance.destroySurfaceKHR(surface);
        device.destroy();
        vkb::destroy_debug_utils_messenger(instance, debugMessenger);
        instance.destroy();
        SDL_DestroyWindow(window);
    }
    isInitialized = false;
}

void Engine::init_vulkan()
{
    // Initialize the vulkan instance
    vkb::InstanceBuilder instBuilder;
    auto instRet = instBuilder.set_app_name(PROJNAME.c_str())
                       .require_api_version(API_VERSION[0], API_VERSION[1], API_VERSION[2])
                       // .enable_extensions({VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME,
                       //                     VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
                       //                     VK_KHR_SURFACE_EXTENSION_NAME})
                       .request_validation_layers(enableValidationLayers)
                       .use_default_debug_messenger()
                       .build();

    vkb::Instance vkbInstance = instRet.value();

    instance = vkbInstance.instance;
    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
    debugMessenger = vkbInstance.debug_messenger;

    // Create a surface
    VkSurfaceKHR cSurface;
    SDL_Vulkan_CreateSurface(window, instance, nullptr, &cSurface);
    surface = vk::SurfaceKHR(cSurface);

    // Set the device features that we want
    vk::PhysicalDeviceVulkan13Features features13{};
    features13.dynamicRendering = vk::True;
    features13.synchronization2 = vk::True;

    vk::PhysicalDeviceVulkan12Features features12{};
    features12.descriptorIndexing = vk::True;
    features12.descriptorBindingUniformBufferUpdateAfterBind = vk::True;
    features12.descriptorBindingPartiallyBound = vk::True;
    features12.runtimeDescriptorArray = vk::True;
    features12.scalarBlockLayout = vk::True;

    vk::PhysicalDeviceSwapchainMaintenance1FeaturesEXT swapchainMaintenance1Features{};
    swapchainMaintenance1Features.setSwapchainMaintenance1(vk::True);

    // Select a GPU
    vkb::PhysicalDeviceSelector physDevSelector{vkbInstance};
    vkb::PhysicalDevice vkbPhysDev
        = physDevSelector.set_minimum_version(API_VERSION[0], API_VERSION[1])
              .set_required_features_13(features13)
              .set_required_features_12(features12)
              // .add_required_extension(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME)
              // .add_required_extension_features(
              //     static_cast<VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT>(
              //         swapchainMaintenance1Features))
              .set_surface(surface)
              .select()
              .value();
    physicalDevice = vkbPhysDev.physical_device;

    // Create the vulkan logical device
    vkb::DeviceBuilder deviceBuilder{vkbPhysDev};
    vkb::Device vkbDevice = deviceBuilder.build().value();
    device = vkbDevice.device;
    VULKAN_HPP_DEFAULT_DISPATCHER.init(device);

    graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    graphicsQueueFamilyIndex = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
}

void Engine::init_swapchain()
{
    create_swapchain(windowExtent.width, windowExtent.height);
}

void Engine::init_commands()
{
    vk::CommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    commandPoolCreateInfo.setQueueFamilyIndex(graphicsQueueFamilyIndex);
    for (int i = 0; i < FRAME_OVERLAP; i++) {
        // Create a command pool per thread
        frames[i].commandPool = device.createCommandPool(commandPoolCreateInfo);
        // Allocate them straight away
        vk::CommandBufferAllocateInfo commandBufferAllocInfo{};
        commandBufferAllocInfo.setCommandPool(frames[i].commandPool);
        commandBufferAllocInfo.setCommandBufferCount(1);
        commandBufferAllocInfo.setLevel(vk::CommandBufferLevel::ePrimary);
        frames[i].mainCommandBuffer = device.allocateCommandBuffers(commandBufferAllocInfo)[0];
    }
}

void Engine::init_sync_structures()
{
    vk::FenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);
    vk::SemaphoreCreateInfo semaphoreCreateInfo{};

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        // gpu->cpu. will force us to wait for the draw commands of a given frame to be finished
        frames[i].renderFence = device.createFence(fenceCreateInfo);
        // gpu->gpu. will control presenting the image to the OS once the drawing finishes
        frames[i].renderSemaphore = device.createSemaphore(semaphoreCreateInfo);
        // gpu->gpu. will make the render commands wait until the swapchain requests the next image
        frames[i].swapchainSemaphore = device.createSemaphore(semaphoreCreateInfo);
    }
}

void Engine::create_swapchain(uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder swapchainBuilder(physicalDevice, device, surface);

    vkb::Swapchain vkbSwapchain
        = swapchainBuilder
              //use vsync present mode
              .set_desired_present_mode(static_cast<VkPresentModeKHR>(PRESENT_MODE))
              .set_desired_extent(width, height)
              .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
              .set_desired_format(
                  VkSurfaceFormatKHR{.format = VK_FORMAT_B8G8R8A8_UNORM,
                                     .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
              .build()
              .value();

    windowExtent = vkbSwapchain.extent;
    swapchain = vkbSwapchain.swapchain;
    swapchainImageFormat = static_cast<vk::Format>(vkbSwapchain.image_format);
    std::vector<VkImage> swapchainImagesC = vkbSwapchain.get_images().value();
    swapchainImages.resize(swapchainImagesC.size());
    for (size_t i = 0; i < swapchainImagesC.size(); i++)
        swapchainImages[i] = static_cast<vk::Image>(swapchainImagesC[i]);
    std::vector<VkImageView> swapchainImageViewsC = vkbSwapchain.get_image_views().value();
    swapchainImageViews.resize(swapchainImageViewsC.size());
    for (size_t i = 0; i < swapchainImageViewsC.size(); i++)
        swapchainImageViews[i] = static_cast<vk::ImageView>(swapchainImageViewsC[i]);
}

void Engine::destroy_swapchain()
{
    device.destroySwapchainKHR(swapchain);
    for (auto imageView : swapchainImageViews) {
        device.destroyImageView(imageView);
    }
    swapchainImageViews.clear();
    swapchainImages.clear();
}

void Engine::run()
{
    VK_CHECK(vk::Result::eErrorTooManyObjects);

    SDL_Event e;
    bool quit = false;

    // Main loop
    while (!quit) {
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_EVENT_QUIT:
                quit = true;
                break;

            case SDL_EVENT_WINDOW_MINIMIZED:
                stopRendering = true;

            case SDL_EVENT_WINDOW_RESTORED:
                stopRendering = false;
            }
        }
        // Do not draw and throttle if we are minimized
        if (stopRendering) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        // Draw if not minimized
        draw();
    }
}

void Engine::draw()
{
    // Wait max 1s until the gpu finished rendering the last frame
    // VK_CHECK(device.waitForFences(get_current_frame().renderFence, vk::True, FENCE_TIMEOUT));
    auto resWaitFences = device.waitForFences(get_current_frame().renderFence,
                                              vk::True,
                                              FENCE_TIMEOUT);
    VK_CHECK(resWaitFences);
    device.resetFences(get_current_frame().renderFence);

    // Request image from the swapchain
    vk::AcquireNextImageInfoKHR acquireImageInfo{};
    acquireImageInfo.setSwapchain(swapchain);
    acquireImageInfo.setSemaphore(get_current_frame().swapchainSemaphore);
    acquireImageInfo.setFence(nullptr);
    acquireImageInfo.setTimeout(FENCE_TIMEOUT);
    acquireImageInfo.setDeviceMask(1); // First and only device in the group

    auto [resNextImage, swapchainImageIndex] = device.acquireNextImage2KHR(acquireImageInfo);
    VK_CHECK(resNextImage);

    // We can safely copy command buffers
    vk::CommandBuffer cmd = get_current_frame().mainCommandBuffer;
    // Thanks to the fence, we are sure now that we can safely reset the commbuff and start recording again.
    cmd.reset();
    vk::CommandBufferBeginInfo commandBufferBeginInfo{};
    commandBufferBeginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    cmd.begin(commandBufferBeginInfo);

    //make the swapchain image into writeable mode before rendering
    utils::transition_image(cmd,
                            swapchainImages[swapchainImageIndex],
                            vk::ImageLayout::eUndefined,
                            vk::ImageLayout::eGeneral);

    // Set a flashing clear value
    vk::ClearColorValue clearValue;
    float flash = std::abs(std::sin(static_cast<float>(frameNumber) / 90.f));
    clearValue = {flash, flash, flash, 1.f};
    vk::ImageSubresourceRange clearRange{};
    clearRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
    clearRange.setLevelCount(vk::RemainingMipLevels);
    clearRange.setLayerCount(vk::RemainingArrayLayers);
    cmd.clearColorImage(swapchainImages[swapchainImageIndex],
                        vk::ImageLayout::eGeneral,
                        clearValue,
                        clearRange);

    // Transition to present on screen
    utils::transition_image(cmd,
                            swapchainImages[swapchainImageIndex],
                            vk::ImageLayout::eGeneral,
                            vk::ImageLayout::ePresentSrcKHR);

    cmd.end();
    // Set the sync objects
    //prepare the submission to the queue.
    //we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
    //we will signal the _renderSemaphore, to signal that rendering has finished
    vk::CommandBufferSubmitInfo cmdInfo{};
    cmdInfo.setDeviceMask(1);
    cmdInfo.setCommandBuffer(cmd);
    vk::SemaphoreSubmitInfo semaphoreWaitInfo{};
    semaphoreWaitInfo.setSemaphore(get_current_frame().swapchainSemaphore);
    semaphoreWaitInfo.setStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput);
    semaphoreWaitInfo.setDeviceIndex(0);
    semaphoreWaitInfo.setValue(1);
    vk::SemaphoreSubmitInfo semaphoreSignalInfo{};
    semaphoreSignalInfo.setSemaphore(get_current_frame().renderSemaphore);
    semaphoreSignalInfo.setStageMask(vk::PipelineStageFlagBits2::eAllGraphics);
    semaphoreSignalInfo.setDeviceIndex(0);
    semaphoreSignalInfo.setValue(1);
    vk::SubmitInfo2 submitInfo{};
    submitInfo.setCommandBufferInfos(cmdInfo);
    submitInfo.setWaitSemaphoreInfos(semaphoreWaitInfo);
    submitInfo.setSignalSemaphoreInfos(semaphoreSignalInfo);

    //submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    graphicsQueue.submit2(submitInfo, get_current_frame().renderFence);

    //prepare present
    // this will put the image we just rendered to into the visible window.
    // we want to wait on the _renderSemaphore for that,
    // as its necessary that drawing commands have finished before the image is displayed to the user
    vk::PresentInfoKHR presentInfo{};
    presentInfo.setSwapchains(swapchain);
    presentInfo.setWaitSemaphores(get_current_frame().renderSemaphore);
    presentInfo.setImageIndices(swapchainImageIndex);

    auto resQueuePresent = graphicsQueue.presentKHR(&presentInfo);
    VK_CHECK(resQueuePresent);

    frameNumber++;
}
