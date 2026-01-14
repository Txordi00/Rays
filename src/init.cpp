#include "init.hpp"
#include "acceleration_structures.hpp"
#include "loader.hpp"
#include "rt_pipelines.hpp"
#include "utils.hpp"

#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <glm/ext/matrix_transform.hpp>
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <print>
#include <stb_image.h>

Init::Init()
{
    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    window = SDL_CreateWindow(PROJNAME, W, H, SDL_WINDOW_VULKAN);

    init_vulkan();

    init_rt();
    create_swapchain(W, H);
    create_camera();
    init_commands();
    init_sync_structures();
    create_draw_data();
    load_meshes();
    load_background();
    // create_lights();
    create_as();
    init_descriptors();
    init_pipelines();
    create_sbt();
    init_imgui();
    presample();

    isInitialized = true;
    std::println("Initialization complete");
}

void Init::clean()
{
    if (isInitialized) {
        device.waitIdle();

        ImGui_ImplVulkan_Shutdown();
        utils::destroy_buffer(allocator, tlas.as.buffer);
        device.destroyAccelerationStructureKHR(tlas.as.AS);
        utils::destroy_buffer(allocator, tlas.instancesBuffer);
        asBuilder->destroy();

        gltfLoader->destroy();
        scene->destroy(device, allocator);

        presampler->destroy();

        // Destroy lights
        // for (auto &l : lights)
        //     l.destroy();

        // device.destroyPipelineLayout(simpleMeshGraphicsPipeline.pipelineLayout);
        // device.destroyPipeline(simpleMeshGraphicsPipeline.pipeline);
        rtPipelineBuilder->destroy();
        device.destroyPipelineLayout(simpleRtPipeline.pipelineLayout);
        // device.destroyPipeline(simpleRtPipeline.pipeline);
        while (!rtPipelineQueue.empty()) {
            device.destroyPipeline(rtPipelineQueue.front());
            rtPipelineQueue.pop();
        }
        while (!rtSBTBufferQueue.empty()) {
            utils::destroy_buffer(allocator, rtSBTBufferQueue.front());
            rtSBTBufferQueue.pop();
        }

        device.destroyDescriptorPool(imguiPool);
        descHelperUAB->destroy();
        descHelperRt->destroy();
        device.destroyDescriptorSetLayout(descriptorSetLayoutUAB);
        device.destroyDescriptorSetLayout(rtDescriptorSetLayout);

        // Destroy things created in this class from here:
        camera.destroy_camera_buffer();

        device.destroyCommandPool(transferCmdPool);
        device.destroyFence(transferFence);
        for (int i = 0; i < frameOverlap; i++) {
            device.destroyCommandPool(frames[i].commandPool);
            device.destroyFence(frames[i].renderFence);
            device.destroySemaphore(frames[i].renderSemaphore);
        }
        for (const auto &sem : swapchainSemaphores)
            device.destroySemaphore(sem);
        destroy_swapchain();
        instance.destroySurfaceKHR(surface);
        utils::destroy_image(device, allocator, backgroundImage);
        for (const auto &f : frames) {
            utils::destroy_image(device, allocator, f.imageDraw);
            utils::destroy_image(device, allocator, f.imageDepth);
        }
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
                       .request_validation_layers(enableValidationLayers)
                       .enable_extensions({VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
                                           VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME})
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
    features12.descriptorBindingStorageBufferUpdateAfterBind = vk::True;
    features12.descriptorBindingStorageImageUpdateAfterBind = vk::True;
    features12.descriptorBindingSampledImageUpdateAfterBind = vk::True;
    features12.shaderSampledImageArrayNonUniformIndexing = vk::True;
    features12.shaderUniformBufferArrayNonUniformIndexing = vk::True;
    features12.shaderStorageBufferArrayNonUniformIndexing = vk::True;
    features12.shaderStorageImageArrayNonUniformIndexing = vk::True;
    features12.descriptorBindingPartiallyBound = vk::True;
    features12.runtimeDescriptorArray = vk::True;
    features12.scalarBlockLayout = vk::True;
    features12.bufferDeviceAddress = vk::True;

    // NOT SUPPORTED YET! ENABLE AS IT GETS SUPPORTED
    vk::PhysicalDeviceUnifiedImageLayoutsFeaturesKHR unifiedImageLayoutsFeatures{};
    unifiedImageLayoutsFeatures.setUnifiedImageLayouts(vk::True);
    // unifiedImageLayoutsFeatures.setUnifiedImageLayoutsVideo(vk::True);

    std::vector<const char *> rtExtensions = {VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
                                              VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                                              VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
                                              VK_KHR_RAY_TRACING_MAINTENANCE_1_EXTENSION_NAME};
    vk::PhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{};
    asFeatures.setAccelerationStructure(vk::True);
    // asFeatures.setDescriptorBindingAccelerationStructureUpdateAfterBind(vk::True);
    vk::PhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures{};
    rtPipelineFeatures.setRayTracingPipeline(vk::True);

    vk::PhysicalDeviceSwapchainMaintenance1FeaturesKHR swapchainMaintenanceFeatures{};
    swapchainMaintenanceFeatures.setSwapchainMaintenance1(vk::True);

    // vk::PhysicalDeviceFeatures pdFeatures{};
    // pdFeatures.setRobustBufferAccess(vk::True);
    // vk::PhysicalDeviceRobustness2FeaturesKHR robustnessFeatures{};
    // robustnessFeatures.setRobustBufferAccess2(vk::True);

    // Select a GPU
    vkb::PhysicalDeviceSelector physDevSelector{vkbInstance};

    auto resSelector = physDevSelector.set_minimum_version(API_VERSION[0], API_VERSION[1])
                           .set_required_features_13(features13)
                           .set_required_features_12(features12)
                           .add_required_extension(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME)
                           // .add_required_extension(VK_KHR_ROBUSTNESS_2_EXTENSION_NAME)
                           .add_required_extension(
                               VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME) // NOT SUPPORTED YET
                           .add_required_extension_features(unifiedImageLayoutsFeatures)
                           .add_required_extensions(rtExtensions)
                           .add_required_extension_features(asFeatures)
                           .add_required_extension_features(rtPipelineFeatures)
                           // .set_required_features(pdFeatures)
                           // .add_required_extension_features(robustnessFeatures)
                           .add_required_extension_features(swapchainMaintenanceFeatures)
                           .set_surface(surface)
                           .select();

    // std::cout << resSelector.error() << std::endl;
    vkb::PhysicalDevice vkbPhysDev = resSelector.value();
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
              .set_desired_min_image_count(2)
              .build()
              .value();

    swapchainExtent = vkbSwapchain.extent;
    swapchain = vkbSwapchain.swapchain;

    // Get the swapchain VkObjects and convert them to vk::Objects
    swapchainImageFormat = static_cast<vk::Format>(vkbSwapchain.image_format);
    std::vector<VkImage> swapchainImagesC = vkbSwapchain.get_images().value();
    std::vector<VkImageView> swapchainImageViewsC = vkbSwapchain.get_image_views().value();
    swapchainImages.resize(swapchainImagesC.size());
    for (size_t i = 0; i < swapchainImagesC.size(); i++) {
        swapchainImages[i].image = static_cast<vk::Image>(swapchainImagesC[i]);
        swapchainImages[i].imageView = static_cast<vk::ImageView>(swapchainImageViewsC[i]);
        swapchainImages[i].format = swapchainImageFormat;
    }

    // Allocate the semaphores vector as well
    swapchainSemaphores.resize(swapchainImagesC.size());
    // Select the number of frames that we are going to process per thread
    frameOverlap = FRAME_OVERLAP;
    frames.resize(frameOverlap);
}

void Init::create_draw_data()
{
    // Same extent as the window
    vk::Extent3D drawExtent{swapchainExtent, 1};

    // We need ColorAttachment for the graphics pipeline and Storage for the RT pipeline
    constexpr vk::ImageUsageFlags drawUsageFlags = vk::ImageUsageFlagBits::eTransferDst
                                                   | vk::ImageUsageFlagBits::eTransferSrc
                                                   | vk::ImageUsageFlagBits::eStorage
                                                   | vk::ImageUsageFlagBits::eColorAttachment;
    constexpr vk::ImageUsageFlags depthUsageFlags = vk::ImageUsageFlagBits::eDepthStencilAttachment;

    // Overkill format
    for (auto &f : frames) {
        f.imageDraw.format = vk::Format::eR32G32B32A32Sfloat;
        f.imageDraw.extent = drawExtent;

        f.imageDraw = utils::create_image(device,
                                          allocator,
                                          f.mainCommandBuffer,
                                          f.renderFence,
                                          graphicsQueue,
                                          vk::Format::eR32G32B32A32Sfloat,
                                          drawUsageFlags,
                                          drawExtent);

        f.imageDepth = utils::create_image(device,
                                           allocator,
                                           f.mainCommandBuffer,
                                           f.renderFence,
                                           graphicsQueue,
                                           vk::Format::eD32Sfloat,
                                           depthUsageFlags,
                                           drawExtent);
    }
}

void Init::create_camera()
{
    camera = Camera{};
    camera.setProjMatrix(FOV,
                         static_cast<float>(swapchainExtent.width),
                         static_cast<float>(swapchainExtent.height),
                         0.01f,
                         100.f);
    camera.create_camera_buffer(device, allocator);
    camera.backwards(3.f);
    camera.up(1.f);
    camera.lookAt(glm::vec3(0.f));
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
    }
    // gpu->gpu. will make the render commands wait until the swapchain requests the next image
    for (int i = 0; i < swapchainSemaphores.size(); i++) {
        swapchainSemaphores[i] = device.createSemaphore(semaphoreCreateInfo);
    }

    transferFence = device.createFence(fenceCreateInfo);
}

void Init::init_descriptors()
{
    descHelperUAB = std::make_unique<DescHelper>(device,
                                                 physicalDeviceProperties,
                                                 asProperties,
                                                 true);
    descHelperUAB
        ->add_descriptor_set(vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer,
                                                    static_cast<uint32_t>(
                                                        scene->surfaceUniformBuffers.size())},
                             frameOverlap); // per-surface storage
    descHelperUAB->add_descriptor_set(vk::DescriptorPoolSize{vk::DescriptorType::eSampler,
                                                             static_cast<uint32_t>(
                                                                 scene->samplers.size())},
                                      frameOverlap); // samplers (usually only one)
    descHelperUAB->add_descriptor_set(vk::DescriptorPoolSize{vk::DescriptorType::eSampledImage,
                                                             static_cast<uint32_t>(
                                                                 scene->images.size())},
                                      frameOverlap); // images to sample
    descHelperUAB->add_descriptor_set(vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer,
                                                             MAX_LIGHTS},
                                      frameOverlap); // Lights
    descHelperUAB->create_descriptor_pool();
    descHelperUAB->add_binding(
        Binding{vk::DescriptorType::eUniformBuffer,
                vk::ShaderStageFlagBits::eClosestHitKHR,
                0,
                static_cast<uint32_t>(scene->surfaceUniformBuffers.size())}); // per-surface storage
    descHelperUAB->add_binding(Binding{vk::DescriptorType::eSampler,
                                       vk::ShaderStageFlagBits::eClosestHitKHR,
                                       1,
                                       static_cast<uint32_t>(scene->samplers.size())}); // samplers
    descHelperUAB->add_binding(
        Binding{vk::DescriptorType::eSampledImage,
                vk::ShaderStageFlagBits::eClosestHitKHR,
                2,
                static_cast<uint32_t>(scene->images.size())}); // sampled images
    descHelperUAB->add_binding(Binding{vk::DescriptorType::eUniformBuffer,
                                       vk::ShaderStageFlagBits::eClosestHitKHR,
                                       3,
                                       MAX_LIGHTS}); // lights
    descriptorSetLayoutUAB = descHelperUAB->create_descriptor_set_layout();
    std::vector<vk::DescriptorSet> setsUAB
        = descHelperUAB->allocate_descriptor_sets(descriptorSetLayoutUAB, frameOverlap);
    for (int i = 0; i < setsUAB.size(); i++)
        frames[i].descriptorSetUAB = setsUAB[i];

    descHelperRt = std::make_unique<DescHelper>(device,
                                                physicalDeviceProperties,
                                                asProperties,
                                                false);
    descHelperRt
        ->add_descriptor_set(vk::DescriptorPoolSize{vk::DescriptorType::eAccelerationStructureKHR,
                                                    1},
                             frameOverlap); // tlas
    descHelperRt->add_descriptor_set(vk::DescriptorPoolSize{vk::DescriptorType::eStorageImage, 1},
                                     frameOverlap); // drawImage
    descHelperRt->add_descriptor_set(vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, 1},
                                     frameOverlap); // camera
    descHelperRt
        ->add_descriptor_set(vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, 1},
                             frameOverlap); // Presampling hemisphere
    descHelperRt
        ->add_descriptor_set(vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, 1},
                             frameOverlap); // Presampling ggx
    descHelperRt
        ->add_descriptor_set(vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, 1},
                             frameOverlap); // Env map
    descHelperRt->create_descriptor_pool();
    descHelperRt->add_binding(
        Binding{vk::DescriptorType::eAccelerationStructureKHR,
                vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR,
                0}); // tlas
    descHelperRt->add_binding(Binding{vk::DescriptorType::eStorageImage,
                                      vk::ShaderStageFlagBits::eRaygenKHR,
                                      1}); // drawImage
    descHelperRt->add_binding(Binding{vk::DescriptorType::eUniformBuffer,
                                      vk::ShaderStageFlagBits::eRaygenKHR,
                                      2}); // camera
    descHelperRt->add_binding(Binding{vk::DescriptorType::eCombinedImageSampler,
                                      vk::ShaderStageFlagBits::eClosestHitKHR,
                                      3}); // presampling hemisphere
    descHelperRt->add_binding(Binding{vk::DescriptorType::eCombinedImageSampler,
                                      vk::ShaderStageFlagBits::eClosestHitKHR,
                                      4}); // presampling ggx
    descHelperRt->add_binding(Binding{vk::DescriptorType::eCombinedImageSampler,
                                      vk::ShaderStageFlagBits::eMissKHR,
                                      5}); // Env map

    rtDescriptorSetLayout = descHelperRt->create_descriptor_set_layout();
    std::vector<vk::DescriptorSet> setsRt
        = descHelperRt->allocate_descriptor_sets(rtDescriptorSetLayout, frameOverlap);
    for (int i = 0; i < setsRt.size(); i++)
        frames[i].descriptorSetRt = setsRt[i];
}

void Init::init_pipelines()
{
    rtPipelineBuilder = std::make_unique<RtPipelineBuilder>(device);
    rtPipelineBuilder->create_shader_stages();
    rtPipelineBuilder->create_shader_groups();
    std::vector<vk::DescriptorSetLayout> descLayouts = {rtDescriptorSetLayout,
                                                        descriptorSetLayoutUAB};
    simpleRtPipeline.pipelineLayout = rtPipelineBuilder->buildPipelineLayout(descLayouts);
    simpleRtPipeline.pipeline = rtPipelineBuilder->buildPipeline(simpleRtPipeline.pipelineLayout);
    rtPipelineQueue.push(simpleRtPipeline.pipeline);
}

void Init::rebuid_rt_pipeline(const SpecializationConstantsClosestHit &constantsCH,
                              const SpecializationConstantsMiss &constantsMiss)
{
    vk::Pipeline newPipeline = rtPipelineBuilder->buildPipeline(simpleRtPipeline.pipelineLayout,
                                                                constantsCH,
                                                                constantsMiss);
    simpleRtPipeline.pipeline = newPipeline;
    rtSBTBuffer = sbtHelper->create_shader_binding_table(newPipeline);
    rtPipelineQueue.push(newPipeline);
    rtSBTBufferQueue.push(rtSBTBuffer);
    assert(rtPipelineQueue.size() == rtSBTBufferQueue.size());
    if (rtPipelineQueue.size() > 2) {
        vk::Pipeline pipelineToDestroy = rtPipelineQueue.front();
        Buffer bufferToDestroy = rtSBTBufferQueue.front();
        device.destroyPipeline(pipelineToDestroy);
        utils::destroy_buffer(allocator, bufferToDestroy);
        rtPipelineQueue.pop();
        rtSBTBufferQueue.pop();
    }

    if (rtProperties.maxRayRecursionDepth < constantsCH.recursionDepth)
        throw std::runtime_error("Driver recursion depth not enough. Driver: "
                                 + std::to_string(rtProperties.maxRayRecursionDepth)
                                 + ". Required: " + std::to_string(constantsCH.recursionDepth + 1));
}

void Init::create_sbt()
{
    sbtHelper = std::make_unique<SbtHelper>(device, allocator, rtProperties);
    rtSBTBuffer = sbtHelper->create_shader_binding_table(simpleRtPipeline.pipeline);
    rtSBTBufferQueue.push(rtSBTBuffer);
}

void Init::init_imgui()
{
    // Create descriptor pool for IMGUI
    std::vector<vk::DescriptorPoolSize> poolSizes = {
        {vk::DescriptorType::eCombinedImageSampler,
         IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE}};

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
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = swapchainImages.size();
    initInfo.UseDynamicRendering = true;

    //dynamic rendering parameters for imgui to use
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo
        = VkPipelineRenderingCreateInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                                        .pNext = nullptr};
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats
        = (VkFormat *) &swapchainImageFormat;

    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

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

void Init::presample()
{
    presampler = std::make_unique<Presampler>(device,
                                              allocator,
                                              cmdTransfer,
                                              transferQueue,
                                              transferFence);
    presampler->run();
}

void Init::load_meshes()
{
    gltfLoader = std::make_unique<GLTFLoader>(device, allocator, transferQueueFamilyIndex);
    // scene = gltfLoader->load_gltf_asset("/home/jordi/Documents/lrt/assets/CornellBox-Original.gltf")
    //             .value();
    scene = gltfLoader->load_gltf_asset("/home/jordi/Documents/lrt/assets/ABeautifulGame.glb")
                .value();
    std::println("Asset loaded");

    glm::mat4 S = glm::scale(1.f * glm::vec3(1.f));
    glm::mat4 R = glm::rotate(glm::pi<float>(), glm::vec3(0.f, 0.f, 1.f));
    // glm::mat4 T = glm::translate(glm::vec3(0.f, 2.f, 20.f));
    for (const auto &n : scene->topNodes)
        n->refreshTransform(R * S);
}

void Init::load_background()
{
    int w, h, c;
    const std::filesystem::path fsPath{
        "/home/jordi/Documents/lrt/assets/rogland_clear_night_4k.hdr"};
    stbi_uc *imData = stbi_load(fsPath.c_str(), &w, &h, &c, 4);
    vk::Extent3D imSize{};
    imSize.setWidth(w);
    imSize.setHeight(h);
    imSize.setDepth(1);

    backgroundImage = utils::create_image(device,
                                          allocator,
                                          cmdTransfer,
                                          transferFence,
                                          transferQueue,
                                          vk::Format::eR8G8B8A8Unorm,
                                          vk::ImageUsageFlagBits::eSampled,
                                          imSize,
                                          imData);

    vk::SamplerCreateInfo samplerCreate{};
    samplerCreate.setMaxLod(vk::LodClampNone);
    samplerCreate.setMinLod(0.f);
    samplerCreate.setMagFilter(vk::Filter::eLinear);
    samplerCreate.setMinFilter(vk::Filter::eLinear);
    samplerCreate.setMipmapMode(vk::SamplerMipmapMode::eLinear);
    backgroundImage.sampler = device.createSampler(samplerCreate);

    stbi_image_free(imData);
}

// void Init::create_lights()
// {
//     Light directionalLight{device, allocator};
//     directionalLight.lightData.positionOrDirection = glm::normalize(glm::vec3(1.f, 1.f, 1.f));
//     directionalLight.lightData.intensity = 1.f;
//     directionalLight.lightData.type = LightType::eDirectional;
//     directionalLight.upload();
//     // const glm::vec3 c{0.f, 0.f, 20.f};
//     // const float r = 5.f;
//     // const float y = -5.f;
//     // const uint32_t n = 0;
//     // lights.reserve(n + 1);
//     lights.push_back(directionalLight);
//     // for (uint32_t i = 0; i < n; i++) {
//     //     const float in = static_cast<float>(i) / static_cast<float>(n);
//     //     const float x = r * cos(2.f * glm::pi<float>() * in);
//     //     const float z = r * sin(2.f * glm::pi<float>() * in);
//     //     Light pointLight{device, allocator};
//     //     pointLight.lightData.positionOrDirection = c + glm::vec3(x, y, z);
//     //     pointLight.lightData.intensity = 40.f;
//     //     pointLight.lightData.type = LightType::ePoint;
//     //     pointLight.upload();
//     //     lights.emplace_back(pointLight);
//     // }
// }

void Init::create_as()
{
    asBuilder = std::make_unique<ASBuilder>(device,
                                            allocator,
                                            graphicsQueueFamilyIndex,
                                            asProperties);
    tlas = asBuilder->buildTLAS(scene);
}

void Init::destroy_swapchain()
{
    device.destroySwapchainKHR(swapchain);
    for (const auto &sc : swapchainImages) {
        device.destroyImageView(sc.imageView);
    }
    swapchainImages.clear();
}
