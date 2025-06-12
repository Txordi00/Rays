#ifndef USE_CXX20_MODULES
#else
import vulkan_hpp;
#include <vulkan/vulkan_hpp_macros.hpp>
#endif

#include "engine.hpp"
#include "types.hpp"
#include "utils.hpp"
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <cmath>
#include <memory>
#include <thread>
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <imgui/imgui.h>
#include <imgui/imgui_impl_sdl3.h>
#include <imgui/imgui_impl_vulkan.h>
#include <vk_mem_alloc.h>

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
    create_swapchain(W, H);
    create_draw_data();
    init_commands();
    init_sync_structures();
    init_descriptors();
    init_pipelines();
    init_imgui();

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

        ImGui_ImplVulkan_Shutdown();
        device.destroyDescriptorPool(imguiPool);
        for (int i = 0; i < pipelines.size(); i++) {
            device.destroyPipelineLayout(pipelines[i].pipelineLayout);
            device.destroyPipeline(pipelines[i].pipeline);
        }
        descriptorPool->destroyPool();
        device.destroyDescriptorSetLayout(drawImageDescriptorsData.layout);

        for (int i = 0; i < frameOverlap; i++) {
            device.destroyCommandPool(frames[i].commandPool);
            device.destroyFence(frames[i].renderFence);
            device.destroySemaphore(frames[i].renderSemaphore);
            device.destroySemaphore(frames[i].swapchainSemaphore);
        }
        destroy_swapchain();
        instance.destroySurfaceKHR(surface);
        device.destroyImageView(imageDraw.imageView);
        vmaDestroyImage(allocator, imageDraw.image, imageDraw.allocation);
        vmaDestroyAllocator(allocator);
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
    physicalDeviceProperties = vkbPhysDev.properties;

    // Create the vulkan logical device
    vkb::DeviceBuilder deviceBuilder{vkbPhysDev};
    vkb::Device vkbDevice = deviceBuilder.build().value();
    device = vkbDevice.device;
    // Final step of initialization of the dynamic dispatcher
    VULKAN_HPP_DEFAULT_DISPATCHER.init(device);

    // Load the
    graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    graphicsQueueFamilyIndex = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    // Initialization of the VMA Allocator
    VmaVulkanFunctions vulkanFunctions{};
    vulkanFunctions.vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr;
    // vulkanFunctions.vkCreateImage = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateImage;

    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.instance = instance;
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;
    // allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocatorInfo.pVulkanFunctions = &vulkanFunctions;
    vmaCreateAllocator(&allocatorInfo, &allocator);
}

void Engine::create_draw_data()
{
    // Same extent as the window
    vk::Extent3D drawExtent{swapchainExtent, 1};

    // Overkill format
    imageDraw.format = vk::Format::eR16G16B16A16Sfloat;
    imageDraw.extent = drawExtent;

    // Probably have to add Storage and Color Attachment later on
    vk::ImageUsageFlags drawUsageFlags = vk::ImageUsageFlagBits::eTransferDst
                                         | vk::ImageUsageFlagBits::eTransferSrc
                                         | vk::ImageUsageFlagBits::eStorage
                                         | vk::ImageUsageFlagBits::eColorAttachment;

    vk::ImageCreateInfo imageCreateInfo = utils::init::image_create_info(imageDraw.format,
                                                                         drawUsageFlags,
                                                                         drawExtent);

    VmaAllocationCreateInfo allocationCreateInfo{};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    // allocationCreateInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Efficiently create and allocate the image with VMA
    vmaCreateImage(allocator,
                   (VkImageCreateInfo *) &imageCreateInfo,
                   &allocationCreateInfo,
                   (VkImage *) &imageDraw.image,
                   &imageDraw.allocation,
                   nullptr);

    // imageDraw.image = drawImage_c;
    // Create the handle vk::ImageView. Not possible to do this with VMA
    vk::ImageViewCreateInfo imageViewCreateInfo
        = utils::init::image_view_create_info(imageDraw.format,
                                              imageDraw.image,
                                              vk::ImageAspectFlagBits::eColor);
    imageDraw.imageView = device.createImageView(imageViewCreateInfo);
}

void Engine::init_commands()
{
    vk::CommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    commandPoolCreateInfo.setQueueFamilyIndex(graphicsQueueFamilyIndex);
    for (int i = 0; i < frameOverlap; i++) {
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

    for (int i = 0; i < frameOverlap; i++) {
        // gpu->cpu. will force us to wait for the draw commands of a given frame to be finished
        frames[i].renderFence = device.createFence(fenceCreateInfo);
        // gpu->gpu. will control presenting the image to the OS once the drawing finishes
        frames[i].renderSemaphore = device.createSemaphore(semaphoreCreateInfo);
        // gpu->gpu. will make the render commands wait until the swapchain requests the next image
        frames[i].swapchainSemaphore = device.createSemaphore(semaphoreCreateInfo);
    }
}

void Engine::init_imgui()
{
    // 1: create descriptor pool for IMGUI
    //  the size of the pool is very oversize, but it's copied from imgui demo
    //  itself.
    std::vector<vk::DescriptorPoolSize> poolSizes = {
        {vk::DescriptorType::eCombinedImageSampler,
         IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE},
    };
    // std::vector<vk::DescriptorPoolSize> poolSizes
    //     = {{vk::DescriptorType::eSampler, 1000},
    //        {vk::DescriptorType::eCombinedImageSampler, 1000},
    //        {vk::DescriptorType::eSampledImage, 1000},
    //        {vk::DescriptorType::eStorageImage, 1000},
    //        {vk::DescriptorType::eUniformTexelBuffer, 1000},
    //        {vk::DescriptorType::eStorageTexelBuffer, 1000},
    //        {vk::DescriptorType::eUniformBuffer, 1000},
    //        {vk::DescriptorType::eStorageBuffer, 1000},
    //        {vk::DescriptorType::eUniformBufferDynamic, 1000},
    //        {vk::DescriptorType::eStorageBufferDynamic, 1000},
    //        {vk::DescriptorType::eInputAttachment, 1000}};

    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
    unsigned int maxSets = 0;
    for (vk::DescriptorPoolSize &poolSize : poolSizes)
        maxSets += poolSize.descriptorCount;
    poolInfo.setMaxSets(maxSets);
    poolInfo.setPoolSizes(poolSizes);

    try {
        imguiPool = device.createDescriptorPool(poolInfo);
    } catch (const std::exception &e) {
        VK_CHECK_EXC(e);
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;
    ImGui_ImplSDL3_InitForVulkan(window);

    // this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = physicalDevice;
    initInfo.Device = device;
    initInfo.Queue = graphicsQueue;
    initInfo.DescriptorPool = imguiPool;
    initInfo.MinImageCount = MINIMUM_FRAME_OVERLAP;
    initInfo.ImageCount = frameOverlap;
    initInfo.UseDynamicRendering = true;

    //dynamic rendering parameters for imgui to use
    initInfo.PipelineRenderingCreateInfo
        = VkPipelineRenderingCreateInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                                        .pNext = nullptr};
    initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = (VkFormat *) &swapchainImageFormat;

    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo);

    // ImGui_ImplVulkan_CreateFontsTexture();
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
              .set_desired_min_image_count(MINIMUM_FRAME_OVERLAP)
              .build()
              .value();

    swapchainExtent = vkbSwapchain.extent;
    swapchain = vkbSwapchain.swapchain;

    // Get the swapchain VkObjects and convert them to vk::Objects
    swapchainImageFormat = static_cast<vk::Format>(vkbSwapchain.image_format);
    std::vector<VkImage> swapchainImagesC = vkbSwapchain.get_images().value();
    swapchainImages.resize(swapchainImagesC.size());
    for (size_t i = 0; i < swapchainImagesC.size(); i++)
        swapchainImages[i] = static_cast<vk::Image>(swapchainImagesC[i]);
    std::vector<VkImageView> swapchainImageViewsC = vkbSwapchain.get_image_views().value();
    swapchainImageViews.resize(swapchainImageViewsC.size());
    for (size_t i = 0; i < swapchainImageViewsC.size(); i++)
        swapchainImageViews[i] = static_cast<vk::ImageView>(swapchainImageViewsC[i]);

    // Select the number of frames that we are going to process per thread
    frameOverlap = std::max(vkbSwapchain.image_count, MINIMUM_FRAME_OVERLAP);
    frames.resize(frameOverlap);
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

void Engine::init_descriptors()
{
    // Set the descriptor set properties for the gradient compute shader layout
    vk::DescriptorType drawImageDescriptorSetType = vk::DescriptorType::eStorageImage;
    DescriptorSetLayout drawImageDescSetLayout{device};
    vk::DescriptorSetLayoutBinding binding{};
    binding.setBinding(0);
    binding.setDescriptorType(drawImageDescriptorSetType);
    binding.setStageFlags(vk::ShaderStageFlagBits::eCompute);
    binding.setDescriptorCount(1);
    drawImageDescSetLayout.add_binding(binding);

    vk::DescriptorSetLayoutCreateFlags descSetLayoutCreateFlags{};
    drawImageDescriptorsData.layout = drawImageDescSetLayout.get_descriptor_set_layout(
        descSetLayoutCreateFlags);
    drawImageDescriptorsData.type = drawImageDescriptorSetType;
    drawImageDescriptorsData.descriptorCount = 10;

    // I need to play with the maxSets parameter!
    descriptorPool = std::make_unique<DescriptorPool>(device,
                                                      std::vector<DescriptorSetData>{
                                                          drawImageDescriptorsData},
                                                      9);
    vk::DescriptorPoolCreateFlags descriptorPoolCreateFlags{};
    descriptorPool->create(descriptorPoolCreateFlags);

    drawImageDescriptors = descriptorPool->allocate_descriptors({0})[0];

    vk::DescriptorImageInfo imageInfo{};
    imageInfo.setImageLayout(vk::ImageLayout::eGeneral);
    imageInfo.setImageView(imageDraw.imageView);

    vk::WriteDescriptorSet descriptorImageDrawWrite{};
    descriptorImageDrawWrite.setDstBinding(0);
    descriptorImageDrawWrite.setImageInfo(imageInfo);
    descriptorImageDrawWrite.setDescriptorType(drawImageDescriptorsData.type);
    descriptorImageDrawWrite.setDescriptorCount(1);
    descriptorImageDrawWrite.setDstSet(drawImageDescriptors);

    device.updateDescriptorSets(descriptorImageDrawWrite, nullptr);
}

void Engine::init_pipelines()
{
    init_background_compute_pipeline();
}

void Engine::init_background_compute_pipeline()
{
    pipelines.resize(2);

    pipelines[0].name = "gradientcolor";

    vk::PushConstantRange pushColors{};
    pushColors.setOffset(0);
    pushColors.setSize(sizeof(GradientColorPush));
    pushColors.setStageFlags(vk::ShaderStageFlagBits::eCompute);

    vk::PipelineLayoutCreateInfo gradColorComputeLayoutCreateInfo{};
    gradColorComputeLayoutCreateInfo.setSetLayouts(drawImageDescriptorsData.layout);
    gradColorComputeLayoutCreateInfo.setPushConstantRanges(pushColors);
    pipelines[0].pipelineLayout = device.createPipelineLayout(gradColorComputeLayoutCreateInfo);

    vk::ShaderModule gradColorComputeShader = utils::load_shader(device,
                                                                 GRADIENT_COLOR_COMP_SHADER_FP);

    vk::PipelineShaderStageCreateInfo gradColorComputeStageCreate{};
    gradColorComputeStageCreate.setModule(gradColorComputeShader);
    gradColorComputeStageCreate.setPName("main");
    gradColorComputeStageCreate.setStage(vk::ShaderStageFlagBits::eCompute);

    vk::ComputePipelineCreateInfo gradColorComputePipelineCreate{};
    gradColorComputePipelineCreate.setLayout(pipelines[0].pipelineLayout);
    gradColorComputePipelineCreate.setStage(gradColorComputeStageCreate);

    vk::Result res;
    vk::Pipeline val;
    try {
        std::tie(res, val) = device.createComputePipeline(nullptr, gradColorComputePipelineCreate);
        pipelines[0].pipeline = val;
    } catch (const std::exception &e) {
        VK_CHECK_EXC(e);
    }
    VK_CHECK_RES(res);

    static GradientColorPush defaultGradColorPush{};
    defaultGradColorPush.colorUp = glm::vec4{1., 0., 0., 1.};
    defaultGradColorPush.colorDown = glm::vec4{0., 1., 0., 1.};
    pipelines[0].pushData = (void *) &defaultGradColorPush;
    pipelines[0].pushDataSize = sizeof(GradientColorPush);

    device.destroyShaderModule(gradColorComputeShader);

    pipelines[1].name = "sky";

    vk::PushConstantRange pushSky{};
    pushSky.setOffset(0);
    pushSky.setSize(sizeof(SkyPush));
    pushSky.setStageFlags(vk::ShaderStageFlagBits::eCompute);

    vk::PipelineLayoutCreateInfo skyComputeLayoutCreateInfo{};
    skyComputeLayoutCreateInfo.setSetLayouts(drawImageDescriptorsData.layout);
    skyComputeLayoutCreateInfo.setPushConstantRanges(pushSky);
    pipelines[1].pipelineLayout = device.createPipelineLayout(skyComputeLayoutCreateInfo);

    vk::ShaderModule skyComputeShader = utils::load_shader(device, SKY_SHADER_FP);

    vk::PipelineShaderStageCreateInfo skyComputeStageCreate{};
    skyComputeStageCreate.setModule(skyComputeShader);
    skyComputeStageCreate.setPName("main");
    skyComputeStageCreate.setStage(vk::ShaderStageFlagBits::eCompute);

    vk::ComputePipelineCreateInfo skyComputePipelineCreate{};
    skyComputePipelineCreate.setLayout(pipelines[1].pipelineLayout);
    skyComputePipelineCreate.setStage(skyComputeStageCreate);

    try {
        std::tie(res, val) = device.createComputePipeline(nullptr, skyComputePipelineCreate);
        pipelines[1].pipeline = val;
    } catch (const std::exception &e) {
        VK_CHECK_EXC(e);
    }
    VK_CHECK_RES(res);

    static SkyPush skyPush{};
    skyPush.colorW = glm::vec4{0.1, 0.2, 0.4, 0.97};
    pipelines[1].pushData = (void *) &skyPush;
    pipelines[1].pushDataSize = sizeof(SkyPush);

    device.destroyShaderModule(skyComputeShader);
}

void Engine::run()
{
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
            //send SDL event to imgui for handling
            ImGui_ImplSDL3_ProcessEvent(&e);
        }
        // Do not draw and throttle if we are minimized
        if (stopRendering) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        // imgui new frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        if (ImGui::Begin("background")) {
            PipelineData &currentPipeline = pipelines[currentPipelineIndex];
            const std::string text = "Selected background compute shader: " + currentPipeline.name;
            ImGui::Text("%s", text.c_str());
            ImGui::SliderInt("Shader Index: ", &currentPipelineIndex, 0, pipelines.size() - 1);
            glm::vec4 *colorUp = (glm::vec4 *) pipelines[0].pushData;
            glm::vec4 *colorDown = (glm::vec4 *) ((char *) pipelines[0].pushData
                                                  + sizeof(glm::vec4));
            ImGui::InputFloat4("[ColorGradient] colorUp", (float *) colorUp);
            ImGui::InputFloat4("[ColorGradient] colorDown", (float *) colorDown);
            ImGui::InputFloat4("[Sky] colorW", (float *) pipelines[1].pushData);
        }
        ImGui::End();
        //some imgui UI to test
        // ImGui::ShowDemoWindow();

        //make imgui calculate internal draw structures
        ImGui::Render();
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
    VK_CHECK_RES(resWaitFences);
    device.resetFences(get_current_frame().renderFence);

    // Request image from the swapchain
    vk::AcquireNextImageInfoKHR acquireImageInfo{};
    acquireImageInfo.setSwapchain(swapchain);
    acquireImageInfo.setSemaphore(get_current_frame().swapchainSemaphore);
    acquireImageInfo.setFence(nullptr);
    acquireImageInfo.setTimeout(FENCE_TIMEOUT);
    acquireImageInfo.setDeviceMask(1); // First and only device in the group

    auto [resNextImage, swapchainImageIndex] = device.acquireNextImage2KHR(acquireImageInfo);
    VK_CHECK_RES(resNextImage);

    // We can safely copy command buffers
    vk::CommandBuffer cmd = get_current_frame().mainCommandBuffer;
    // Thanks to the fence, we are sure now that we can safely reset cmd and start recording again.
    cmd.reset();

    // Record the next set of commands
    vk::CommandBufferBeginInfo commandBufferBeginInfo{};
    commandBufferBeginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    cmd.begin(commandBufferBeginInfo);

    // Make the draw image into writeable mode before rendering
    utils::transition_image(cmd,
                            imageDraw.image,
                            vk::ImageLayout::eUndefined,
                            vk::ImageLayout::eGeneral);
    // Draw into the draw image
    change_background(cmd);

    // Transition the draw image to be sent and the swapchain image to receive it
    utils::transition_image(cmd,
                            imageDraw.image,
                            vk::ImageLayout::eGeneral,
                            vk::ImageLayout::eTransferSrcOptimal);
    utils::transition_image(cmd,
                            swapchainImages[swapchainImageIndex],
                            vk::ImageLayout::eUndefined,
                            vk::ImageLayout::eTransferDstOptimal);

    // Copy draw to swapchain
    utils::copy_image(cmd,
                      imageDraw.image,
                      swapchainImages[swapchainImageIndex],
                      vk::Extent2D{imageDraw.extent.width, imageDraw.extent.height},
                      swapchainExtent);

    utils::transition_image(cmd,
                            swapchainImages[swapchainImageIndex],
                            vk::ImageLayout::eTransferDstOptimal,
                            vk::ImageLayout::eColorAttachmentOptimal);

    draw_imgui(cmd, swapchainImageViews[swapchainImageIndex]);

    utils::transition_image(cmd,
                            swapchainImages[swapchainImageIndex],
                            vk::ImageLayout::eColorAttachmentOptimal,
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
    VK_CHECK_RES(resQueuePresent);

    frameNumber++;
}

void Engine::change_background(vk::CommandBuffer &cmd)
{
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipelines[currentPipelineIndex].pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                           pipelines[currentPipelineIndex].pipelineLayout,
                           0,
                           drawImageDescriptors,
                           nullptr);
    cmd.pushConstants(pipelines[currentPipelineIndex].pipelineLayout,
                      vk::ShaderStageFlagBits::eCompute,
                      0,
                      pipelines[currentPipelineIndex].pushDataSize,
                      pipelines[currentPipelineIndex].pushData);
    cmd.dispatch(std::ceil((float) imageDraw.extent.width / 16.f),
                 std::ceil((float) imageDraw.extent.height / 16.f),
                 1);
}

void Engine::draw_imgui(const vk::CommandBuffer &cmd, const vk::ImageView &imageView)
{
    vk::RenderingAttachmentInfo colorAttachmentInfo{};
    colorAttachmentInfo.setImageView(imageView);
    colorAttachmentInfo.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal);
    colorAttachmentInfo.setLoadOp(vk::AttachmentLoadOp::eLoad);
    colorAttachmentInfo.setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingInfo renderInfo{};
    renderInfo.setColorAttachments(colorAttachmentInfo);
    renderInfo.setLayerCount(1);
    renderInfo.setRenderArea(vk::Rect2D{vk::Offset2D{0, 0}, swapchainExtent});

    cmd.beginRendering(renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    cmd.endRendering();
}
