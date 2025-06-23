#ifndef USE_CXX20_MODULES
#else
import vulkan_hpp;
#include <vulkan/vulkan_hpp_macros.hpp>
#endif

#include "engine.hpp"
#include "pipelines_compute.hpp"
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
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_RADIANS
#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
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

    window = SDL_CreateWindow(PROJNAME, W, H, SDL_WINDOW_VULKAN);

    init_vulkan();
    create_swapchain(W, H);
    create_draw_data();
    init_commands();
    init_sync_structures();
    init_descriptors();
    init_pipelines();
    init_imgui();
    load_meshes();

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
        for (auto &m : gpuMeshes) {
            destroy_buffer(m->meshBuffer.vertexBuffer);
            destroy_buffer(m->meshBuffer.indexBuffer);
        }
        device.destroyPipelineLayout(simpleMeshGraphicsPipeline.pipelineLayout);
        device.destroyPipeline(simpleMeshGraphicsPipeline.pipeline);
        device.destroyDescriptorPool(imguiPool);
        for (int i = 0; i < computePipelines.size(); i++) {
            device.destroyPipelineLayout(computePipelines[i].pipelineLayout);
            device.destroyPipeline(computePipelines[i].pipeline);
        }
        descriptorPool->destroyPool();
        device.destroyDescriptorSetLayout(drawImageDescriptorsData.layout);

        device.destroyCommandPool(transferCmdPool);
        device.destroyFence(transferFence);
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
    auto instRet = instBuilder.set_app_name(PROJNAME)
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
    features12.bufferDeviceAddress = vk::True;

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
    transferQueue = vkbDevice.get_queue(vkb::QueueType::transfer).value();
    transferQueueFamilyIndex = vkbDevice.get_queue_index(vkb::QueueType::transfer).value();

    // Initialization of the VMA Allocator
    VmaVulkanFunctions vulkanFunctions{};
    vulkanFunctions.vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr;
    // vulkanFunctions.vkCreateImage = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateImage;

    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.instance = instance;
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
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
    vk::CommandPoolCreateInfo transferCommandPoolCreateInfo{};
    transferCommandPoolCreateInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    transferCommandPoolCreateInfo.setQueueFamilyIndex(transferQueueFamilyIndex);
    transferCmdPool = device.createCommandPool(transferCommandPoolCreateInfo);
    vk::CommandBufferAllocateInfo transferBufferAllocInfo{};
    transferBufferAllocInfo.setCommandPool(transferCmdPool);
    transferBufferAllocInfo.setCommandBufferCount(1);
    transferBufferAllocInfo.setLevel(vk::CommandBufferLevel::ePrimary);
    cmdTransfer = device.allocateCommandBuffers(transferBufferAllocInfo)[0];
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

    transferFence = device.createFence(fenceCreateInfo);
}

void Engine::load_meshes()
{
    GLTFLoader gltfLoader{};
    gltfLoader.overrideColorsWithNormals = true;
    std::vector<std::shared_ptr<HostMeshAsset>> cpuMeshes = gltfLoader.loadGLTFMeshes(
        "../../assets/basicmesh.glb");
    gpuMeshes.resize(cpuMeshes.size());
    for (int i = 0; i < cpuMeshes.size(); i++) {
        gpuMeshes[i] = std::make_shared<DeviceMeshAsset>(cpuMeshes[i]->name,
                                                         cpuMeshes[i]->surfaces,
                                                         create_mesh(cpuMeshes[i]->indices,
                                                                     cpuMeshes[i]->vertices));
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
    computePipelines = init_background_compute_pipelines(device, drawImageDescriptorsData.layout);
    simpleMeshGraphicsPipeline = get_simple_mesh_pipeline(device, imageDraw.format);
}

Buffer Engine::create_buffer(const vk::DeviceSize &size,
                             const vk::BufferUsageFlags &usageFlags,
                             const VmaMemoryUsage &memoryUsage,
                             const VmaAllocationCreateFlags &allocationFlags)
{
    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.setUsage(usageFlags);
    bufferInfo.setSize(size);

    VmaAllocationCreateInfo vmaallocInfo{};
    vmaallocInfo.usage = memoryUsage;
    vmaallocInfo.flags = allocationFlags;

    Buffer createdBuffer;
    VK_CHECK_RES(vmaCreateBuffer(allocator,
                                 (VkBufferCreateInfo *) &bufferInfo,
                                 &vmaallocInfo,
                                 (VkBuffer *) &createdBuffer.buffer,
                                 &createdBuffer.allocation,
                                 &createdBuffer.allocationInfo));

    return createdBuffer;
}

void Engine::destroy_buffer(const Buffer &buffer)
{
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

// OPTIMIZATION: This could be run on a separate thread in order to not force the main thread to wait
// for fences
MeshBuffer Engine::create_mesh(const std::span<uint32_t> &indices, const std::span<Vertex> &vertices)
{
    const vk::DeviceSize verticesSize = vertices.size() * sizeof(Vertex);
    const vk::DeviceSize indicesSize = indices.size() * sizeof(uint32_t);

    MeshBuffer mesh;

    mesh.vertexBuffer = create_buffer(verticesSize,
                                      vk::BufferUsageFlagBits::eStorageBuffer
                                          | vk::BufferUsageFlagBits::eShaderDeviceAddress
                                          | vk::BufferUsageFlagBits::eTransferDst,
                                      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    // | VMA_ALLOCATION_CREATE_MAPPED_BIT);
    vk::BufferDeviceAddressInfo vertexAddressInfo{};
    vertexAddressInfo.setBuffer(mesh.vertexBuffer.buffer);
    mesh.vertexBufferAddress = device.getBufferAddress(vertexAddressInfo);

    mesh.indexBuffer = create_buffer(indicesSize,
                                     vk::BufferUsageFlagBits::eIndexBuffer
                                         | vk::BufferUsageFlagBits::eTransferDst,
                                     VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                                     0);

    Buffer stagingBuffer = create_buffer(verticesSize + indicesSize,
                                         vk::BufferUsageFlagBits::eTransferSrc,
                                         VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                                         VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                             | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    void *stagingData = stagingBuffer.allocation->GetMappedData();

    memcpy(stagingData, vertices.data(), verticesSize);
    memcpy((char *) stagingData + verticesSize, indices.data(), indicesSize);

    // Set info structures to copy from staging to vertex & index buffers
    vk::CopyBufferInfo2 vertexCopyInfo{};
    vertexCopyInfo.setSrcBuffer(stagingBuffer.buffer);
    vertexCopyInfo.setDstBuffer(mesh.vertexBuffer.buffer);
    vk::BufferCopy2 vertexCopy{};
    vertexCopy.setSrcOffset(0);
    vertexCopy.setDstOffset(0);
    vertexCopy.setSize(verticesSize);
    vertexCopyInfo.setRegions(vertexCopy);

    vk::CopyBufferInfo2 indexCopyInfo{};
    indexCopyInfo.setSrcBuffer(stagingBuffer.buffer);
    indexCopyInfo.setDstBuffer(mesh.indexBuffer.buffer);
    vk::BufferCopy2 indexCopy{};
    indexCopy.setSrcOffset(verticesSize);
    indexCopy.setDstOffset(0);
    indexCopy.setSize(indicesSize);
    indexCopyInfo.setRegions(indexCopy);

    // New command buffer to copy in GPU from staging buffer to vertex & index buffers
    device.resetFences(transferFence);

    vk::CommandBufferBeginInfo cmdBeginInfo{};
    cmdBeginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    cmdTransfer.begin(cmdBeginInfo);
    cmdTransfer.copyBuffer2(vertexCopyInfo);
    cmdTransfer.copyBuffer2(indexCopyInfo);
    cmdTransfer.end();

    vk::CommandBufferSubmitInfo cmdSubmitInfo{};
    cmdSubmitInfo.setCommandBuffer(cmdTransfer);
    cmdSubmitInfo.setDeviceMask(1);
    vk::SubmitInfo2 submitInfo{};
    submitInfo.setCommandBufferInfos(cmdSubmitInfo);

    try {
        transferQueue.submit2(submitInfo, transferFence);
        VK_CHECK_RES(device.waitForFences(transferFence, vk::True, FENCE_TIMEOUT));
    } catch (const std::exception &e) {
        VK_CHECK_EXC(e);
    }
    destroy_buffer(stagingBuffer);

    return mesh;
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
            ComputePipelineData &currentPipeline = computePipelines[currentBackgroundPipelineIndex];
            const std::string text = "Selected background compute shader: " + currentPipeline.name;
            ImGui::Text("%s", text.c_str());
            ImGui::SliderInt("Shader Index: ",
                             &currentBackgroundPipelineIndex,
                             0,
                             computePipelines.size() - 1);
            glm::vec4 *colorUp = (glm::vec4 *) computePipelines[0].pushData;
            glm::vec4 *colorDown = (glm::vec4 *) ((char *) computePipelines[0].pushData
                                                  + sizeof(glm::vec4));
            ImGui::InputFloat4("[ColorGradient] colorUp", (float *) colorUp);
            ImGui::InputFloat4("[ColorGradient] colorDown", (float *) colorDown);
            ImGui::InputFloat4("[Sky] colorW", (float *) computePipelines[1].pushData);
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

    utils::transition_image(cmd,
                            imageDraw.image,
                            vk::ImageLayout::eGeneral,
                            vk::ImageLayout::eColorAttachmentOptimal);

    draw_meshes(cmd);

    // Transition the draw image to be sent and the swapchain image to receive it
    utils::transition_image(cmd,
                            imageDraw.image,
                            vk::ImageLayout::eColorAttachmentOptimal,
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

void Engine::change_background(const vk::CommandBuffer &cmd)
{
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute,
                     computePipelines[currentBackgroundPipelineIndex].pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                           computePipelines[currentBackgroundPipelineIndex].pipelineLayout,
                           0,
                           drawImageDescriptors,
                           nullptr);
    cmd.pushConstants(computePipelines[currentBackgroundPipelineIndex].pipelineLayout,
                      vk::ShaderStageFlagBits::eCompute,
                      0,
                      computePipelines[currentBackgroundPipelineIndex].pushDataSize,
                      computePipelines[currentBackgroundPipelineIndex].pushData);
    cmd.dispatch(std::ceil((float) imageDraw.extent.width / 16.f),
                 std::ceil((float) imageDraw.extent.height / 16.f),
                 1);
}

void Engine::draw_meshes(const vk::CommandBuffer &cmd)
{
    vk::RenderingAttachmentInfo colorAttachmentInfo{};
    colorAttachmentInfo.setImageView(imageDraw.imageView);
    colorAttachmentInfo.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal);
    colorAttachmentInfo.setLoadOp(vk::AttachmentLoadOp::eLoad);
    colorAttachmentInfo.setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingInfo renderInfo{};
    renderInfo.setColorAttachments(colorAttachmentInfo);
    renderInfo.setLayerCount(1);
    renderInfo.setRenderArea(vk::Rect2D{vk::Offset2D{0, 0}, swapchainExtent});

    //set dynamic viewport and scissor
    vk::Viewport viewport{};
    viewport.setX(0.f);
    viewport.setY(0.f);
    viewport.setWidth(swapchainExtent.width);
    viewport.setHeight(swapchainExtent.height);
    viewport.setMinDepth(0.f);
    viewport.setMaxDepth(1.f);

    vk::Rect2D scissor{};
    scissor.setExtent(vk::Extent2D{swapchainExtent.width, swapchainExtent.height});
    scissor.setOffset(vk::Offset2D{0, 0});

    try {
        cmd.beginRendering(renderInfo);
        cmd.setViewport(0, viewport);
        cmd.setScissor(0, scissor);

        int objId = 2;
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, simpleMeshGraphicsPipeline.pipeline);

        // The order is translate -> rotate -> scale.
        // I am confused about the Z-axis here. Why translating in +z makes objects bigger
        // and not smaller?
        glm::mat4 modelMat = glm::translate(glm::mat4(1.f),
                                            glm::vec3(static_cast<float>(frameNumber) * 0.01,
                                                      0.f,
                                                      -5.f));
        // I implement the rotations as quaternions and I accumulate them in quaternion space
        glm::quat rotQuat = glm::angleAxis(glm::radians(static_cast<float>(frameNumber % 360)),
                                           glm::vec3(0, 0, -1));
        // After that I generate the rotation matrix with them. Would be more efficient to send directly
        // the quaternions + the translations to the shader?
        glm::mat4 rotMat = glm::toMat4(rotQuat);
        // This becomes modelMat = T * R
        modelMat *= rotMat;
        // modelMat = T * R * S
        modelMat = glm::scale(modelMat, glm::vec3(0.5f));

        // The order is the opposite since we are actually writing an inverse matrix here.
        // I don't think that it makes sense to do any scaling in the view matrix
        glm::quat viewPitchQuat = glm::angleAxis(glm::radians(0.f), glm::vec3(1, 0, 0));
        // Rotate the camera slightly to the left. The angle is -10 instead of +10 because
        // the inverse of a rotation \theta is a rotation -\theta
        glm::quat viewYawQuat = glm::angleAxis(glm::radians(-10.f), glm::vec3(0, 1, 0));
        glm::quat viewRollQuat = glm::angleAxis(glm::radians(0.f), glm::vec3(0, 0, -1));
        // No need to normalize. As with complex numbers, the product of quaternions
        // is unitary
        glm::quat viewRotQuat = viewPitchQuat * viewYawQuat * viewRollQuat;
        glm::mat4 viewRotMat = glm::toMat4(viewRotQuat);
        // I am not sure about wether this translation should go here or before the rotation...
        glm::mat4 viewMat = glm::translate(viewRotMat, glm::vec3(0.f, 0.f, -.5f));

        // Point-projection matrix. It's cool that GLM has a simple method to write it!
        glm::mat4 projMat = glm::perspective(glm::radians(70.f),
                                             static_cast<float>(swapchainExtent.width)
                                                 / static_cast<float>(swapchainExtent.height),
                                             0.01f,
                                             100.f);

        // Final matrix that I send to the vertex shader
        glm::mat4 mvpMatrix = projMat * viewMat * modelMat;

        MeshPush pushConstants;
        pushConstants.worldMatrix = mvpMatrix;
        pushConstants.vertexBufferAddress = gpuMeshes[objId]->meshBuffer.vertexBufferAddress;
        cmd.pushConstants(simpleMeshGraphicsPipeline.pipelineLayout,
                          vk::ShaderStageFlagBits::eVertex,
                          0,
                          sizeof(MeshPush),
                          &pushConstants);
        cmd.bindIndexBuffer(gpuMeshes[objId]->meshBuffer.indexBuffer.buffer,
                            0,
                            vk::IndexType::eUint32);
        cmd.drawIndexed(gpuMeshes[objId]->surfaces[0].count,
                        1,
                        gpuMeshes[objId]->surfaces[0].startIndex,
                        0,
                        0);
    } catch (const std::exception &e) {
        VK_CHECK_EXC(e);
    }
    cmd.endRendering();
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
