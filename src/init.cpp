#include "init.hpp"
#include "acceleration_structures.hpp"
#include "loader.hpp"
#include "loader2.hpp"
#include "rt_pipelines.hpp"
#include "utils.hpp"

#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <glm/ext/matrix_transform.hpp>
#include <unordered_set>
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_sdl3.h>
#include <imgui/imgui_impl_vulkan.h>
#include <print>

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
    create_as();
    init_descriptors();
    init_pipelines();
    create_sbt();
    init_imgui();

    isInitialized = true;
}

void Init::clean()
{
    if (isInitialized) {
        device.waitIdle();

        ImGui_ImplVulkan_Shutdown();
        // for (auto &m : models) {
        //     m->destroyBuffers();
        // }
        utils::destroy_buffer(allocator, rtSBTBuffer);
        utils::destroy_buffer(allocator, tlas.buffer);
        device.destroyAccelerationStructureKHR(tlas.AS);

        device.destroyPipelineLayout(simpleMeshGraphicsPipeline.pipelineLayout);
        device.destroyPipeline(simpleMeshGraphicsPipeline.pipeline);
        device.destroyPipelineLayout(simpleRtPipeline.pipelineLayout);
        device.destroyPipeline(simpleRtPipeline.pipeline);

        device.destroyDescriptorPool(imguiPool);
        descHelperUAB->destroy();
        descHelperRt->destroy();
        device.destroyDescriptorSetLayout(descriptorSetLayoutUAB);
        device.destroyDescriptorSetLayout(rtDescriptorSetLayout);

        // Destroy things created in this class from here:
        camera.destroy_camera_storage_buffer();

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
        for (const auto &f : frames) {
            device.destroyImageView(f.imageDraw.imageView);
            vmaDestroyImage(allocator, f.imageDraw.image, f.imageDraw.allocation);
            device.destroyImageView(f.imageDepth.imageView);
            vmaDestroyImage(allocator, f.imageDepth.image, f.imageDepth.allocation);
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
    features12.shaderStorageBufferArrayNonUniformIndexing = vk::True;
    features12.descriptorBindingPartiallyBound = vk::True;
    features12.runtimeDescriptorArray = vk::True;
    features12.scalarBlockLayout = vk::True;
    features12.bufferDeviceAddress = vk::True;

    // NOT SUPPORTED YET! ENABLE AS IT GETS SUPPORTED
    vk::PhysicalDeviceUnifiedImageLayoutsFeaturesKHR unifiedImageLayoutsFeatures{};
    unifiedImageLayoutsFeatures.setUnifiedImageLayouts(vk::True);
    unifiedImageLayoutsFeatures.setUnifiedImageLayoutsVideo(vk::True);

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
              .set_desired_min_image_count(MINIMUM_FRAME_OVERLAP)
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
    frameOverlap = MINIMUM_FRAME_OVERLAP;
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
    camera.create_camera_storage_buffer(device, allocator);
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
    // descHelperUAB = std::make_unique<DescHelper>(device,
    //                                              physicalDeviceProperties,
    //                                              asProperties,
    //                                              true);
    // descHelperUAB->add_descriptor_set(vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer,
    //                                                          static_cast<uint32_t>(models.size())},
    //                                   frameOverlap);
    // descHelperUAB->add_descriptor_set(vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer,
    //                                                          static_cast<uint32_t>(models.size())},
    //                                   frameOverlap);
    // descHelperUAB->create_descriptor_pool();
    // descHelperUAB->add_binding(
    //     Binding{vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, 0}); // ubo
    // descHelperUAB->add_binding(
    //     Binding{vk::DescriptorType::eStorageBuffer,
    //             vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eClosestHitKHR,
    //             1});
    // uboDescriptorSetLayout = descHelperUAB->create_descriptor_set_layout();
    // std::vector<vk::DescriptorSet> setsUAB
    //     = descHelperUAB->allocate_descriptor_sets(uboDescriptorSetLayout, frameOverlap);
    // for (int i = 0; i < setsUAB.size(); i++)
    //     frames[i].descriptorSetUAB = setsUAB[i];

    descHelperUAB = std::make_unique<DescHelper>(device,
                                                 physicalDeviceProperties,
                                                 asProperties,
                                                 true);
    descHelperUAB->add_descriptor_set(vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer,
                                                             static_cast<uint32_t>(
                                                                 scene->surfaceStorageBuffersCount)},
                                      frameOverlap); // per-surface storage
    descHelperUAB->create_descriptor_pool();
    descHelperUAB->add_binding(Binding{vk::DescriptorType::eStorageBuffer,
                                       vk::ShaderStageFlagBits::eClosestHitKHR,
                                       0}); // per-surface storage
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
                             frameOverlap);
    descHelperRt->add_descriptor_set(vk::DescriptorPoolSize{vk::DescriptorType::eStorageImage, 1},
                                     frameOverlap);
    // descHelperRt->add_descriptor_set(vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer, 1},
    //                                  frameOverlap);
    descHelperRt->add_descriptor_set(vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, 1},
                                     frameOverlap); // camera
    descHelperRt->create_descriptor_pool();
    descHelperRt->add_binding(
        Binding{vk::DescriptorType::eAccelerationStructureKHR,
                vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR,
                0});
    descHelperRt->add_binding(
        Binding{vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenKHR, 1});
    descHelperRt->add_binding(
        Binding{vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eRaygenKHR, 2});
    rtDescriptorSetLayout = descHelperRt->create_descriptor_set_layout();
    std::vector<vk::DescriptorSet> setsRt
        = descHelperRt->allocate_descriptor_sets(rtDescriptorSetLayout, frameOverlap);
    for (int i = 0; i < setsRt.size(); i++)
        frames[i].descriptorSetRt = setsRt[i];
}

void Init::init_pipelines()
{
    simpleMeshGraphicsPipeline = get_simple_mesh_pipeline(device,
                                                          frames[0].imageDraw.format,
                                                          frames[0].imageDepth.format,
                                                          {descriptorSetLayoutUAB});

    RtPipelineBuilder rtPipelineBuilder{device};
    rtPipelineBuilder.create_shader_stages();
    rtPipelineBuilder.create_shader_groups();
    std::vector<vk::DescriptorSetLayout> descLayouts = {rtDescriptorSetLayout,
                                                        descriptorSetLayoutUAB};
    simpleRtPipeline.pipelineLayout = rtPipelineBuilder.buildPipelineLayout(descLayouts);
    simpleRtPipeline.pipeline = rtPipelineBuilder.buildPipeline(simpleRtPipeline.pipelineLayout);
}

void Init::create_sbt()
{
    sbtHelper = std::make_unique<SbtHelper>(device, allocator, rtProperties);
    rtSBTBuffer = sbtHelper->create_shader_binding_table(simpleRtPipeline.pipeline);
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
    if (rtProperties.maxRayRecursionDepth < MAX_RT_RECURSION)
        throw std::runtime_error("Driver recursion depth not enough. Driver: "
                                 + std::to_string(rtProperties.maxRayRecursionDepth)
                                 + ". Required: " + std::to_string(MAX_RT_RECURSION));
}

void Init::load_meshes()
{
    // std::vector<Material> materials;
    // Material m1{};
    // m1.color = glm::vec3{1.f, 0.f, 0.f};
    // m1.diffuseR = 1.f;
    // m1.specularR = 1.f;
    // m1.shininessN = 4;
    // m1.ambientR = 1.f;
    // m1.reflectiveness = 1.f;
    // m1.refractiveness = 0.f;
    // m1.refractiveIndex = 1.f;
    // utils::normalize_material_factors(m1);
    // materials.push_back(m1);
    // Material m2{};
    // m2.color = glm::vec3{1.f, 1.f, 1.f};
    // m2.diffuseR = 0.2f;
    // m2.specularR = 0.6f;
    // m2.ambientR = 1.f;
    // m2.reflectiveness = 0.1f;
    // m2.refractiveness = 1.f;
    // m2.shininessN = 1;
    // m2.refractiveIndex = 0.9f;
    // utils::normalize_material_factors(m2);
    // materials.push_back(m2);
    // Material m3{};
    // m3.color = glm::vec3{0.f, 1.f, 1.f};
    // m3.diffuseR = 0.8f;
    // m3.specularR = 0.2f;
    // m3.shininessN = 1;
    // m3.ambientR = 1.f;
    // m3.reflectiveness = 0.f;
    // m3.refractiveness = 0.f;
    // m3.refractiveIndex = 1.f;
    // utils::normalize_material_factors(m3);
    // materials.push_back(m3);

    // GLTFLoader gltfLoader{};
    // gltfLoader.overrideColorsWithNormals = false;
    // std::vector<std::shared_ptr<HostMeshAsset>> cpuMeshes = gltfLoader.loadGLTFMeshes(
    //     "../../assets/basicmesh.glb");
    // models.resize(cpuMeshes.size());
    // for (int i = 0; i < cpuMeshes.size(); i++) {
    //     models[i] = std::make_shared<Model>(device, *cpuMeshes[i], allocator, materials[i]);
    //     models[i]->create_mesh(cmdTransfer, transferFence, transferQueue);
    // }
    // models[0]->position = glm::vec3(0.f, 2.f, 7.f);
    // models[1]->position = glm::vec3(2.f, 0.f, 7.f);
    // models[2]->position = glm::vec3(-2.f, 0.f, 7.f);

    gltfLoader = std::make_unique<GLTFLoader2>(device, allocator, transferQueueFamilyIndex);
    scene = gltfLoader->load_gltf_asset("../../assets/ABeautifulGame.glb").value();
}

void Init::create_as()
{
    // std::unordered_set<vk::DeviceAddress> uniqueMeshes;
    // uniqueMeshes.reserve(meshNodes.size());
    // size_t count = 0;
    // for (const auto &mn : meshNodes) {
    //     if (uniqueMeshes.insert(mn->mesh->indexBuffer->bufferAddress).second)
    //         count++;
    // }

    std::unique_ptr<ASBuilder> asBuilder = std::make_unique<ASBuilder>(device,
                                                                       allocator,
                                                                       graphicsQueueFamilyIndex,
                                                                       asProperties);

    tlas = asBuilder->buildTLAS(scene);

    // std::shared_ptr<Node> n = std::make_shared<Node>();
    // std::shared_ptr<MeshNode> m = std::make_shared<MeshNode>();
    // std::vector<std::shared_ptr<Node>> v;
    // v.push_back(n);
    // v.push_back(m);
    // std::vector<std::shared_ptr<MeshNode>> vm;
    // vm.reserve(v.size());
    // for (const auto &x : v) {
    //     if (const std::shared_ptr<MeshNode> m = std::dynamic_pointer_cast<MeshNode>(x)) {
    //         vm.push_back(m);
    //     }
    // }

    // std::unique_ptr<ASBuilder> asBuilder = std::make_unique<ASBuilder>(device,
    //                                                                    allocator,
    //                                                                    graphicsQueueFamilyIndex,
    //                                                                    asProperties);
    // std::vector<glm::mat3x4> transforms(models.size());
    // for (int i = 0; i < models.size(); i++)
    //     transforms[i] = glm::mat3x4(
    //         glm::transpose(glm::translate(glm::mat4(1.f), models[i]->position)));

    // tlas = asBuilder->buildTLAS(models, transforms);
}

void Init::destroy_swapchain()
{
    device.destroySwapchainKHR(swapchain);
    for (const auto &sc : swapchainImages) {
        device.destroyImageView(sc.imageView);
    }
    swapchainImages.clear();
}

