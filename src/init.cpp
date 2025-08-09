#include "init.hpp"
#include "loader.hpp"
#include "pipelines_compute.hpp"
#include "utils.hpp"

#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_sdl3.h>
#include <imgui/imgui_impl_vulkan.h>

Init::Init()
{
    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    window = SDL_CreateWindow(PROJNAME, W, H, SDL_WINDOW_VULKAN);

    init_vulkan();
    init_rt();
    create_swapchain(W, H);
    create_draw_data();
    init_commands();
    init_sync_structures();
    init_compute_descriptors();
    init_ub_descriptors();
    init_pipelines();
    init_imgui();
    load_meshes();

    isInitialized = true;
}

void Init::clean()
{
    if (isInitialized) {
        device.waitIdle();

        ImGui_ImplVulkan_Shutdown();
        for (auto &m : models) {
            m->destroyBuffers(allocator);
        }
        for (auto &b : uniformBuffers)
            utils::destroy_buffer(allocator, b);
        device.destroyPipelineLayout(simpleMeshGraphicsPipeline.pipelineLayout);
        device.destroyPipeline(simpleMeshGraphicsPipeline.pipeline);
        device.destroyDescriptorPool(imguiPool);
        for (int i = 0; i < computePipelines.size(); i++) {
            device.destroyPipelineLayout(computePipelines[i].pipelineLayout);
            device.destroyPipeline(computePipelines[i].pipeline);
        }
        descriptorPool->destroyPool();
        ubo->destroy();
        device.destroyDescriptorSetLayout(drawImageDescriptorsData.layout);
        device.destroyDescriptorSetLayout(uboDescriptorSetLayout);

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
        device.destroyImageView(imageDepth.imageView);
        vmaDestroyImage(allocator, imageDepth.image, imageDepth.allocation);
        vmaDestroyAllocator(allocator);
        device.destroy();
        vkb::destroy_debug_utils_messenger(instance, debugMessenger);
        instance.destroy();
        SDL_DestroyWindow(window);
    }
}

void Init::init_vulkan()
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

    // NOT SUPPORTED YET! ENABLE AS IT GETS SUPPORTED
    // vk::PhysicalDeviceUnifiedImageLayoutsFeaturesKHR unifiedImageLayoutsFeatures{};
    // unifiedImageLayoutsFeatures.setUnifiedImageLayouts(vk::False);
    // unifiedImageLayoutsFeatures.setUnifiedImageLayoutsVideo(vk::False);

    std::vector<const char *> rtExtensions = {VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
                                              VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                                              VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME};
    vk::PhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{};
    asFeatures.setAccelerationStructure(vk::True);
    // asFeatures.setDescriptorBindingAccelerationStructureUpdateAfterBind(vk::True);
    vk::PhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures{};
    // Select a GPU
    vkb::PhysicalDeviceSelector physDevSelector{vkbInstance};
    vkb::PhysicalDevice vkbPhysDev
        = physDevSelector.set_minimum_version(API_VERSION[0], API_VERSION[1])
              .set_required_features_13(features13)
              .set_required_features_12(features12)
              // .add_required_extension(VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME) // NOT SUPPORTED YET
              // .add_required_extension_features(unifiedImageLayoutsFeatures)
              // .add_required_extension(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME)
              // .add_required_extension_features(
              //     static_cast<VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT>(
              //         swapchainMaintenance1Features))
              .add_required_extensions(rtExtensions)
              .add_required_extension_features(asFeatures)
              .add_required_extension_features(rtPipelineFeatures)
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

void Init::create_swapchain(uint32_t width, uint32_t height)
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

void Init::create_draw_data()
{
    // Same extent as the window
    vk::Extent3D drawExtent{swapchainExtent, 1};

    // Overkill format
    imageDraw.format = vk::Format::eR16G16B16A16Sfloat;
    imageDraw.extent = drawExtent;

    // We should be able to erase Storage if we get rid of the background compute pipeline
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

    // Create the handle vk::ImageView. Not possible to do this with VMA
    vk::ImageViewCreateInfo imageViewCreateInfo
        = utils::init::image_view_create_info(imageDraw.format,
                                              imageDraw.image,
                                              vk::ImageAspectFlagBits::eColor);
    imageDraw.imageView = device.createImageView(imageViewCreateInfo);

    // The same for the depth image & image view:
    imageDepth.format = vk::Format::eD32Sfloat;
    imageDepth.extent = drawExtent;

    vk::ImageUsageFlags depthUsageFlags = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    vk::ImageCreateInfo depthCreateInfo = utils::init::image_create_info(imageDepth.format,
                                                                         depthUsageFlags,
                                                                         imageDepth.extent);

    VmaAllocationCreateInfo depthAllocationInfo{};
    depthAllocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    vmaCreateImage(allocator,
                   (VkImageCreateInfo *) &depthCreateInfo,
                   &depthAllocationInfo,
                   (VkImage *) &imageDepth.image,
                   &imageDepth.allocation,
                   nullptr);

    vk::ImageViewCreateInfo depthViewInfo
        = utils::init::image_view_create_info(imageDepth.format,
                                              imageDepth.image,
                                              vk::ImageAspectFlagBits::eDepth);

    try {
        imageDepth.imageView = device.createImageView(depthViewInfo);
    } catch (const std::exception &e) {
        VK_CHECK_EXC(e);
    }
}

void Init::init_commands()
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

void Init::init_sync_structures()
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

void Init::init_compute_descriptors()
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

void Init::init_ub_descriptors()
{
    ubo = std::make_unique<Ubo>(device);
    ubo->create_descriptor_pool(physicalDeviceProperties.limits.maxDescriptorSetUniformBuffers,
                                frameOverlap);
    uboDescriptorSetLayout = ubo->create_descriptor_set_layout(vk::ShaderStageFlagBits::eVertex);
    // vk::PushConstantRange pushRange{};
    // pushRange.setStageFlags(vk::ShaderStageFlagBits::eVertex);
    // pushRange.setOffset(0);
    // pushRange.setSize(sizeof(MeshPush));
    // ubo->create_pipeline_layout(pushRange, uboDescriptorSetLayout);
    uboDescriptorSets = ubo->allocate_descriptor_sets(uboDescriptorSetLayout, frameOverlap);

    uniformBuffers.reserve(frameOverlap);
    for (int i = 0; i < frameOverlap; i++) {
        Buffer b = utils::create_buffer(allocator,
                                        vk::DeviceSize(sizeof(UniformData)),
                                        vk::BufferUsageFlagBits::eUniformBuffer,
                                        VMA_MEMORY_USAGE_AUTO,
                                        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                            | VMA_ALLOCATION_CREATE_MAPPED_BIT);
        b.bufferId = i;
        uniformBuffers.emplace_back(b);
    }

    ubo->update_descriptor_sets(uniformBuffers, uboDescriptorSets);
}

void Init::init_pipelines()
{
    computePipelines = init_background_compute_pipelines(device, drawImageDescriptorsData.layout);

    simpleMeshGraphicsPipeline = get_simple_mesh_pipeline(device,
                                                          imageDraw.format,
                                                          imageDepth.format,
                                                          {uboDescriptorSetLayout});
}

void Init::init_imgui()
{
    // Create descriptor pool for IMGUI
    std::vector<vk::DescriptorPoolSize> poolSizes = {
        {vk::DescriptorType::eCombinedImageSampler,
         IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE},
    };

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

void Init::init_rt()
{
    rtProperties.pNext = &asProperties;
    vk::PhysicalDeviceProperties2 physDevProp2{};
    physDevProp2.pNext = &rtProperties;
    physicalDevice.getProperties2(&physDevProp2);
}

void Init::load_meshes()
{
    GLTFLoader gltfLoader{};
    gltfLoader.overrideColorsWithNormals = true;
    std::vector<std::shared_ptr<HostMeshAsset>> cpuMeshes = gltfLoader.loadGLTFMeshes(
        "../../assets/basicmesh.glb");
    models.resize(cpuMeshes.size());
    for (int i = 0; i < cpuMeshes.size(); i++) {
        models[i] = std::make_shared<Model>(*cpuMeshes[i]);
        models[i]->createGpuMesh(device, allocator, cmdTransfer, transferFence, transferQueue);
    }
}

void Init::destroy_swapchain()
{
    device.destroySwapchainKHR(swapchain);
    for (auto imageView : swapchainImageViews) {
        device.destroyImageView(imageView);
    }
    swapchainImageViews.clear();
    swapchainImages.clear();
}

