#ifndef USE_CXX20_MODULES
#else
import vulkan_hpp;
#include <vulkan/vulkan_hpp_macros.hpp>
#endif

#include "acceleration_structures.hpp"
#include "engine.hpp"
#include "lights.hpp"
#include "types.hpp"
#include "utils.hpp"
#include <glm/ext.hpp>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <print>

Engine::Engine()
{
    I = std::make_unique<Init>();

    // Init push constants aaa
    rayPush.clearColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.f);
    // rayPush.nLights = I->lights.size();
    descUpdater = std::make_unique<DescriptorUpdater>(I->device);
    lightsManager = std::make_unique<LightsManager>(I->device, I->allocator);
}

Engine::~Engine()
{
    // Destroy all the remaining lights
    for (Light &l : lightsManager->lights)
        l.destroy();
    I->clean();
}

void Engine::run()
{
    // Inform the shaders about the resources that we are going to use
    update_descriptors();

    SDL_Event e;
    bool quit = false;

    const float dx = 0.5f;
    const float dt = glm::radians(2.f);

    int numKeys;
    const bool *keyStates = SDL_GetKeyboardState(&numKeys);

    // Main loop
    while (!quit) {
        // SDL_Keycode key = 0;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_EVENT_QUIT:
                quit = true;
                break;

            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_MAXIMIZED:
                shouldResize = true;

            case SDL_EVENT_KEY_DOWN:
                // key = e.key.key;
                if (keyStates[SDL_SCANCODE_W])
                    I->camera.forward(dx);
                if (keyStates[SDL_SCANCODE_S])
                    I->camera.backwards(dx);
                if (keyStates[SDL_SCANCODE_A])
                    I->camera.left(dx);
                if (keyStates[SDL_SCANCODE_D])
                    I->camera.right(dx);
                if (keyStates[SDL_SCANCODE_Q])
                    I->camera.down(dx);
                if (keyStates[SDL_SCANCODE_E])
                    I->camera.up(dx);
                if (keyStates[SDL_SCANCODE_UP])
                    I->camera.lookUp(dt);
                if (keyStates[SDL_SCANCODE_DOWN])
                    I->camera.lookDown(dt);
                if (keyStates[SDL_SCANCODE_LEFT])
                    I->camera.lookLeft(dt);
                if (keyStates[SDL_SCANCODE_RIGHT])
                    I->camera.lookRight(dt);
            }
            //send SDL event to imgui for handling
            ImGui_ImplSDL3_ProcessEvent(&e);
        }

        if (shouldResize)
            resize();

        // Run the UI elements
        update_imgui();

        // Draw if not minimized
        draw();
    }
}

void Engine::update_imgui()
{
    static SpecializationConstantsClosestHit constantsCH{};
    static SpecializationConstantsMiss constantsMiss{};
    static bool random{static_cast<bool>(constantsCH.random)},
        presample{static_cast<bool>(constantsCH.presampled)},
        envMap{static_cast<bool>(constantsMiss.envMap)}, dirLightOn{false};
    static int recursionDepth = constantsCH.recursionDepth, numBounces = constantsCH.numBounces;
    static float scale{1.f}, xRot{0.f}, yRot{0.f}, zRot{0.f};
    static std::filesystem::path imPath{std::string(PROJECT_DIR)
                                        + std::string("/assets/rogland_clear_night_4k.hdr")};

    // imgui new frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    const float framerate = ImGui::GetIO().DeltaTime * 1000.f, fps = 1000.0f / framerate;

    ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImVec2 workPos = viewport->WorkPos;
    ImVec2 workSize = viewport->WorkSize;
    ImVec2 windowPos = ImVec2(workPos.x + workSize.x - 10, workPos.y + 10);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.35f);

    static constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
                                              | ImGuiWindowFlags_AlwaysAutoResize
                                              | ImGuiWindowFlags_NoSavedSettings
                                              | ImGuiWindowFlags_NoFocusOnAppearing
                                              | ImGuiWindowFlags_NoNav;

    ImGui::Begin("Performance", nullptr, flags);
    ImGui::Text("%.1f FPS", fps);
    ImGui::Text("%.2f ms", framerate);
    ImGui::End();

    ImGui::Begin("Controls");

    if (ImGui::Button("Recreate sc")) {
        shouldResize = true;
    }

    ImGui::ColorEdit3("Background color", (float *) &rayPush.clearColor);

    ImGui::Separator();

    ImGui::Checkbox("EnvMap", &envMap);
    ImGui::SameLine();
    if (ImGui::Button("Load from file")) {
        const std::filesystem::path newImPath = utils::load_file_from_window({{"HDRI", "hdr"}});
        I->device.waitIdle();
        utils::destroy_image(I->device, I->allocator, I->backgroundImage);
        I->load_background(newImPath);
        descUpdater->clean();
        for (const auto &f : I->frames) {
            descUpdater->add_combined_image(f.descriptorSetRt, 5, {I->backgroundImage});
        }
        descUpdater->update();
        imPath = newImPath;
    }
    if (envMap) {
        ImGui::SameLine();
        ImGui::Text("%s", imPath.c_str());
    }
    ImGui::Checkbox("Random", &random);
    ImGui::Checkbox("Presample", &presample);

    ImGui::InputInt("Maximum recursion depth", &recursionDepth, 1, 1);
    recursionDepth = std::max(recursionDepth, 1);
    ImGui::InputInt("Bounces", &numBounces, 2, 4);
    numBounces = std::max(numBounces, 2);

    if (ImGui::Button("Apply Changes")) {
        constantsCH.recursionDepth = static_cast<uint32_t>(recursionDepth);
        constantsCH.numBounces = static_cast<uint32_t>(numBounces);
        constantsCH.random = static_cast<vk::Bool32>(random);
        constantsCH.presampled = static_cast<vk::Bool32>(presample);

        constantsMiss.envMap = static_cast<vk::Bool32>(envMap);
        I->rebuid_rt_pipeline(constantsCH, constantsMiss);
    }

    ImGui::Separator();

    const float scaleOld{scale};
    if (ImGui::InputFloat("Scale", &scale, 0.1f, 0.5f, "%.2f")) {
        scale = std::max(scale, 0.01f);
        const float ds = scale / scaleOld;
        const glm::mat4 S = glm::scale(glm::mat4{1.f}, glm::vec3(ds));
        I->asBuilder->updateTLAS(I->tlas, S);
    }

    const float xRotOld{xRot};
    if (ImGui::SliderAngle("X-axis Rotation", &xRot, -180.f, 180.f)) {
        const float dr = xRot - xRotOld;
        const glm::mat4 R = glm::rotate(dr, glm::vec3(1.f, 0.f, 0.f));
        I->asBuilder->updateTLAS(I->tlas, R);
    }

    const float yRotOld{yRot};
    if (ImGui::SliderAngle("Y-axis Rotation", &yRot, -180.f, 180.f)) {
        const float dr = yRot - yRotOld;
        const glm::mat4 R = glm::rotate(dr, glm::vec3(0.f, -1.f, 0.f));
        I->asBuilder->updateTLAS(I->tlas, R);
    }

    const float zRotOld{zRot};
    if (ImGui::SliderAngle("Z-axis Rotation", &zRot, -180.f, 180.f)) {
        const float dr = zRot - zRotOld;
        const glm::mat4 R = glm::rotate(dr, glm::vec3(0.f, 0.f, 1.f));
        I->asBuilder->updateTLAS(I->tlas, R);
    }

    lightsManager->run();
    assert(lightsManager->lightBuffers.size() == lightsManager->lights.size());
    bool updateDescriptors = false;
    for (auto &f : I->frames) {
        updateDescriptors = updateDescriptors || (f.lightsCount != lightsManager->lights.size());
        f.lightsCount = lightsManager->lights.size();
    }
    if (updateDescriptors && lightsManager->lightBuffers.size() > 0) {
        descUpdater->clean();
        for (const auto &f : I->frames) {
            descUpdater->add_uniform(f.descriptorSetUAB, 3, lightsManager->lightBuffers);
        }
        descUpdater->update();
    }

    rayPush.nLights = static_cast<uint32_t>(lightsManager->lights.size());

    ImGui::End();

    ImGui::Render();
}

void Engine::update_descriptors()
{
    descUpdater->clean();
    // Set all the resource descriptors at once. We don't need to update again if we don't change any
    // resources
    const static bool withTextures = I->scene->samplers.size() > 0 && I->scene->images.size() > 0;

    for (const auto &frame : I->frames) {
        // Inform the shaders about all the different descriptors
        const vk::DescriptorSet descriptorSetUAB = frame.descriptorSetUAB;
        const vk::DescriptorSet descriptorSetRt = frame.descriptorSetRt;

        descUpdater->add_uniform(descriptorSetUAB, 0, I->scene->surfaceUniformBuffers);
        if (withTextures) {
            descUpdater->add_sampler(descriptorSetUAB, 1, I->scene->samplers);
            descUpdater->add_sampled_image(descriptorSetUAB, 2, I->scene->images);
        }
        if (lightsManager->lightBuffers.size() > 0)
            descUpdater->add_uniform(descriptorSetUAB, 3, lightsManager->lightBuffers);
        descUpdater->add_as(descriptorSetRt, 0, I->tlas.as.AS);
        descUpdater->add_storage_image(descriptorSetRt, 1, {frame.imageDraw});
        descUpdater->add_uniform(descriptorSetRt, 2, {I->camera.cameraBuffer});
        descUpdater->add_combined_image(descriptorSetRt, 3, {I->presampler->hemisphereImage});
        descUpdater->add_combined_image(descriptorSetRt, 4, {I->presampler->ggxImage});
        descUpdater->add_combined_image(descriptorSetRt, 5, {I->backgroundImage});
    }
    descUpdater->update();
}

void Engine::draw()
{
    vk::Semaphore acquireSemaphore = get_current_frame().renderSemaphore;
    vk::Fence frameFence = get_current_frame().renderFence;
    ImageData imageDraw = get_current_frame().imageDraw;

    // Wait max 1s until the gpu finished rendering the last frame
    vk::Result resFence = I->device.waitForFences(frameFence, vk::True, FENCE_TIMEOUT);
    if (resFence != vk::Result::eSuccess) {
        std::println("Skipping frame");
        return;
    }
    // Request image from the swapchain
    vk::AcquireNextImageInfoKHR acquireImageInfo{};
    acquireImageInfo.setSwapchain(I->swapchain);
    acquireImageInfo.setSemaphore(acquireSemaphore);
    acquireImageInfo.setFence(nullptr);
    acquireImageInfo.setTimeout(FENCE_TIMEOUT);
    acquireImageInfo.setDeviceMask(1); // First and only device in the group

    // std::cout << "acquire" << std::endl;
    vk::Result resAcquire = I->device.acquireNextImage2KHR(&acquireImageInfo, &swapchainImageIndex);
    VK_CHECK_RES(resAcquire);
    if (resAcquire == vk::Result::eErrorOutOfDateKHR) {
        shouldResize = true;
        return;
    }

    I->device.resetFences(frameFence);

    vk::Semaphore submitSemaphore = acquireSemaphore; //I->swapchainSemaphores[swapchainImageIndex];

    vk::MemoryBarrier2 barrier{};
    vk::DependencyInfo depInfo{};
    depInfo.setMemoryBarriers(barrier);

    vk::CommandBuffer cmd = get_current_frame().mainCommandBuffer;
    // Thanks to the fence, we are sure now (NOT ANYMORE!?)
    // that we can safely reset cmd and start recording again.
    cmd.reset();

    // Record the next set of commands
    vk::CommandBufferBeginInfo commandBufferBeginInfo{};
    commandBufferBeginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    cmd.begin(commandBufferBeginInfo);

    utils::transition_image(cmd,
                            I->swapchainImages[swapchainImageIndex],
                            vk::ImageLayout::eUndefined,
                            vk::ImageLayout::eGeneral,
                            vk::PipelineStageFlagBits2::eTopOfPipe,
                            vk::PipelineStageFlagBits2::eRayTracingShaderKHR);

    // raster(cmd);
    raytrace(cmd);

    barrier.setSrcStageMask(vk::PipelineStageFlagBits2::eRayTracingShaderKHR);
    barrier.setDstStageMask(vk::PipelineStageFlagBits2::eBlit);
    barrier.setSrcAccessMask(vk::AccessFlagBits2::eMemoryWrite);
    barrier.setDstAccessMask(vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite);
    cmd.pipelineBarrier2(depInfo);

    // Copy draw to swapchain
    utils::copy_image(cmd,
                      imageDraw.image,
                      I->swapchainImages[swapchainImageIndex].image,
                      vk::Extent2D{imageDraw.extent.width, imageDraw.extent.height},
                      I->swapchainExtent);

    barrier.setSrcStageMask(vk::PipelineStageFlagBits2::eBlit);
    barrier.setDstStageMask(vk::PipelineStageFlagBits2::eAllGraphics);
    barrier.setSrcAccessMask(vk::AccessFlagBits2::eMemoryWrite);
    barrier.setDstAccessMask(vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite);
    cmd.pipelineBarrier2(depInfo);

    draw_imgui(cmd, I->swapchainImages[swapchainImageIndex].imageView);

    utils::transition_image(cmd,
                            I->swapchainImages[swapchainImageIndex],
                            vk::ImageLayout::eGeneral,
                            vk::ImageLayout::ePresentSrcKHR,
                            vk::PipelineStageFlagBits2::eAllGraphics,
                            vk::PipelineStageFlagBits2::eBottomOfPipe);

    cmd.end();

    // Set the sync objects
    // prepare the submission to the queue.
    // we want to wait on the acquire semaphore, as that semaphore is signaled when the swapchain is ready
    // we will signal the submitSemaphore, to signal that rendering has finished
    vk::CommandBufferSubmitInfo cmdInfo{};
    cmdInfo.setDeviceMask(1);
    cmdInfo.setCommandBuffer(cmd);
    vk::SemaphoreSubmitInfo semaphoreWaitInfo{};
    semaphoreWaitInfo.setSemaphore(acquireSemaphore);
    semaphoreWaitInfo.setStageMask(vk::PipelineStageFlagBits2::eAllGraphics
                                   | vk::PipelineStageFlagBits2::eRayTracingShaderKHR);
    semaphoreWaitInfo.setDeviceIndex(0);
    semaphoreWaitInfo.setValue(1);
    vk::SemaphoreSubmitInfo semaphoreSignalInfo{};
    semaphoreSignalInfo.setSemaphore(submitSemaphore);
    semaphoreSignalInfo.setStageMask(vk::PipelineStageFlagBits2::eAllGraphics
                                     | vk::PipelineStageFlagBits2::eRayTracingShaderKHR);
    semaphoreSignalInfo.setDeviceIndex(0);
    semaphoreSignalInfo.setValue(1);
    vk::SubmitInfo2 submitInfo{};
    submitInfo.setCommandBufferInfos(cmdInfo);
    submitInfo.setWaitSemaphoreInfos(semaphoreWaitInfo);
    submitInfo.setSignalSemaphoreInfos(semaphoreSignalInfo);

    // submit command buffer to the queue and execute it.
    // BEFORE: frameFence will now block until the graphic commands finish execution
    // NOW: We pass over submit2 and will wait on present to finish during the next draw() execution
    I->graphicsQueue.submit2(submitInfo, nullptr);
    // I->graphicsQueue.submit2(submitInfo, frameFence);

    // prepare present
    // this will put the image we just rendered to into the visible window.
    // we want to wait on the submitSemaphore for that,
    // as its necessary that drawing commands have finished before the image is displayed to the user
    // We signal the frame fence in order for the next execution to know when it's safe to start
    vk::SwapchainPresentFenceInfoKHR swapchainPresentInfo{};
    swapchainPresentInfo.setFences(frameFence);
    swapchainPresentInfo.setSwapchainCount(1);
    vk::PresentInfoKHR presentInfo{};
    presentInfo.setSwapchains(I->swapchain);
    presentInfo.setWaitSemaphores(submitSemaphore);
    presentInfo.setImageIndices(swapchainImageIndex);
    presentInfo.setPNext(&swapchainPresentInfo);

    vk::Result resPresent = I->graphicsQueue.presentKHR(presentInfo);
    VK_CHECK_RES(resPresent);
    if (resPresent == vk::Result::eErrorOutOfDateKHR)
        shouldResize = true;

    frameNumber = (frameNumber + 1) % I->frameOverlap;
}

void Engine::raytrace(const vk::CommandBuffer &cmd)
{
    cmd.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, I->simpleRtPipeline.pipeline);

    vk::DescriptorSet descriptorSetUniform = get_current_frame().descriptorSetUAB;
    vk::DescriptorSet descriptorSetRt = get_current_frame().descriptorSetRt;
    vk::ImageView imageView = get_current_frame().imageDraw.imageView;
    vk::AccelerationStructureKHR tlas = I->tlas.as.AS;

    std::vector<vk::DescriptorSet> descriptorSets = {descriptorSetRt, descriptorSetUniform};
    vk::BindDescriptorSetsInfo bindSetsInfo{};
    bindSetsInfo.setDescriptorSets(descriptorSets);
    bindSetsInfo.setStageFlags(vk::ShaderStageFlagBits::eRaygenKHR
                               | vk::ShaderStageFlagBits::eClosestHitKHR
                               | vk::ShaderStageFlagBits::eMissKHR);
    bindSetsInfo.setLayout(I->simpleRtPipeline.pipelineLayout);
    bindSetsInfo.setFirstSet(0);

    cmd.bindDescriptorSets2(bindSetsInfo);

    vk::PushConstantsInfo pushInfo{};
    pushInfo.setLayout(I->simpleRtPipeline.pipelineLayout);
    pushInfo.setStageFlags(vk::ShaderStageFlagBits::eRaygenKHR
                           | vk::ShaderStageFlagBits::eClosestHitKHR
                           | vk::ShaderStageFlagBits::eMissKHR);
    pushInfo.setSize(sizeof(RayPush));
    pushInfo.setValues<RayPush>(rayPush);
    pushInfo.setOffset(0);
    cmd.pushConstants2(pushInfo);

    I->camera.update();

    cmd.traceRaysKHR(I->sbtHelper->rgenRegion,
                     I->sbtHelper->missRegion,
                     I->sbtHelper->hitRegion,
                     vk::StridedDeviceAddressRegionKHR{},
                     I->swapchainExtent.width,
                     I->swapchainExtent.height,
                     1);
}

void Engine::draw_imgui(const vk::CommandBuffer &cmd, const vk::ImageView &imageView)
{
    vk::RenderingAttachmentInfo colorAttachmentInfo{};
    colorAttachmentInfo.setImageView(imageView);
    colorAttachmentInfo.setImageLayout(vk::ImageLayout::eGeneral);
    colorAttachmentInfo.setLoadOp(vk::AttachmentLoadOp::eLoad);
    colorAttachmentInfo.setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingInfo renderInfo{};
    renderInfo.setColorAttachments(colorAttachmentInfo);
    renderInfo.setLayerCount(1);
    renderInfo.setRenderArea(vk::Rect2D{vk::Offset2D{0, 0}, I->swapchainExtent});

    cmd.beginRendering(renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    cmd.endRendering();
}

void Engine::resize()
{
    I->device.waitIdle();

    I->recreate_swapchain();

    I->recreate_draw_data();
    descUpdater->clean();
    for (const auto &f : I->frames)
        descUpdater->add_storage_image(f.descriptorSetRt, 1, {f.imageDraw});
    descUpdater->update();

    I->recreate_camera();

    std::println("Swapchain, draw data and camera recreated");

    shouldResize = false;
}
