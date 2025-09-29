#ifndef USE_CXX20_MODULES
#else
import vulkan_hpp;
#include <vulkan/vulkan_hpp_macros.hpp>
#endif

#include "acceleration_structures.hpp"
#include "engine.hpp"
#include "types.hpp"
#include "utils.hpp"
#include <algorithm>
#include <glm/ext.hpp>
#include <imgui/imgui_impl_sdl3.h>
#include <imgui/imgui_impl_vulkan.h>
#include <thread>

Engine::Engine()
{
    I = std::make_unique<Init>();

    // Create vector of buffers in an efficient way
    uniformBuffers.reserve(I->models.size());
    std::ranges::transform(I->models,
                           std::back_inserter(uniformBuffers),
                           [](const std::shared_ptr<Model> &m) { return m->uniformBuffer; });

    storageBuffers.reserve(I->models.size());
    std::ranges::transform(I->models,
                           std::back_inserter(storageBuffers),
                           [](const std::shared_ptr<Model> &m) { return m->storageBuffer; });

    // Init push constants
    rayPush.clearColor = glm::vec4(0.f, 0.0f, 0.2f, 1.f);
    rayPush.numObjects = I->models.size();
    rayPush.lightIntensity = 50.f;
    rayPush.lightType = 0;
    rayPush.lightPosition = glm::vec3(0.f, -5.f, 7.f);
}

Engine::~Engine()
{
    I->clean();
}

void Engine::run()
{
    SDL_Event e;
    bool quit = false;

    // Set all the resource descriptors at once. We don't need more if we don't change any
    // resources
    vk::ImageView imageView = I->imageDraw.imageView;
    vk::AccelerationStructureKHR tlas = I->tlas.AS;
    DescriptorUpdater descUpdater{I->device};
    std::vector<Buffer> cameraBuffer = {I->camera.cameraBuffer};
    for (const auto &frame : I->frames) {
        // Inform the shaders about all the different descriptors
        vk::DescriptorSet descriptorSetUniform = frame.descriptorSetUAB;
        vk::DescriptorSet descriptorSetRt = frame.descriptorSetRt;

        descUpdater.add_uniform(descriptorSetUniform, 0, uniformBuffers);
        descUpdater.add_storage(descriptorSetUniform, 1, storageBuffers);
        descUpdater.add_as(descriptorSetRt, 0, tlas);
        descUpdater.add_image(descriptorSetRt, 1, imageView);
        descUpdater.add_uniform(descriptorSetRt, 2, cameraBuffer);
        descUpdater.update();

        // Transition to general the images where we will be drawing.
        // I do it here because we don't have to transition back thanks to VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION,
        // so we will save many transitions.
        vk::CommandBuffer cmd = frame.mainCommandBuffer;
        cmd.reset();
        // Record the next set of commands
        vk::CommandBufferBeginInfo commandBufferBeginInfo{};
        commandBufferBeginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        cmd.begin(commandBufferBeginInfo);
        utils::transition_image(cmd,
                                I->imageDraw.image,
                                vk::ImageLayout::eUndefined,
                                vk::ImageLayout::eGeneral,
                                vk::PipelineStageFlagBits2::eTopOfPipe,
                                vk::PipelineStageFlagBits2::eBottomOfPipe);
        cmd.end();
        vk::CommandBufferSubmitInfo cmdInfo{};
        cmdInfo.setDeviceMask(1);
        cmdInfo.setCommandBuffer(cmd);
        vk::SubmitInfo2 submitInfo{};
        submitInfo.setCommandBufferInfos(cmdInfo);
        I->graphicsQueue.submit2(submitInfo, nullptr);
    }

    const float dx = 0.5f;
    const float dt = glm::radians(2.f);
    for (const auto &m : I->models)
        m->updateModelMatrix();

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

            case SDL_EVENT_WINDOW_MINIMIZED:
                stopRendering = true;

            case SDL_EVENT_WINDOW_RESTORED:
                stopRendering = false;

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
        // Do not draw and throttle if we are minimized
        if (stopRendering) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        // imgui new frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // if (ImGui::Begin("background")) {
        //     ComputePipelineData &currentPipeline
        //         = I->computePipelines[currentBackgroundPipelineIndex];
        //     const std::string text = "Selected background compute shader: " + currentPipeline.name;
        //     ImGui::Text("%s", text.c_str());
        //     ImGui::SliderInt("Shader Index: ",
        //                      &currentBackgroundPipelineIndex,
        //                      0,
        //                      I->computePipelines.size() - 1);
        //     glm::vec4 *colorUp = (glm::vec4 *) I->computePipelines[0].pushData;
        //     glm::vec4 *colorDown = (glm::vec4 *) ((char *) I->computePipelines[0].pushData
        //                                           + sizeof(glm::vec4));
        //     ImGui::InputFloat4("[ColorGradient] colorUp", (float *) colorUp);
        //     ImGui::InputFloat4("[ColorGradient] colorDown", (float *) colorDown);
        //     ImGui::InputFloat4("[Sky] colorW", (float *) I->computePipelines[1].pushData);
        // }
        // Imgui UI to test
        // ImGui::End();
        ImGui::ShowDemoWindow();

        //make imgui calculate internal draw structures
        ImGui::Render();
        // Draw if not minimized
        draw();
    }
}

void Engine::draw()
{
    vk::Semaphore acquireSemaphore = get_current_frame().renderSemaphore;
    vk::Fence frameFence = get_current_frame().renderFence;

    // Wait max 1s until the gpu finished rendering the last frame
    vk::Result res = I->device.waitForFences(frameFence, vk::True, FENCE_TIMEOUT);
    if (res != vk::Result::eSuccess) {
        std::cout << "Skipping frame" << std::endl;
        // return;
    }
    // std::cout << "resetfences" << std::endl;
    I->device.resetFences(frameFence);

    // Request image from the swapchain
    vk::AcquireNextImageInfoKHR acquireImageInfo{};
    acquireImageInfo.setSwapchain(I->swapchain);
    acquireImageInfo.setSemaphore(acquireSemaphore);
    acquireImageInfo.setFence(nullptr);
    acquireImageInfo.setTimeout(FENCE_TIMEOUT);
    acquireImageInfo.setDeviceMask(1); // First and only device in the group

    // std::cout << "acquire" << std::endl;
    VK_CHECK_RES(I->device.acquireNextImage2KHR(&acquireImageInfo, &swapchainImageIndex));

    vk::Semaphore submitSemaphore = I->swapchainSemaphores[swapchainImageIndex];

    vk::MemoryBarrier2 barrier{};
    barrier.srcStageMask = vk::PipelineStageFlagBits2::eAllCommands;
    barrier.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands;
    barrier.srcAccessMask = vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead;
    barrier.dstAccessMask = vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead;
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

    // Instead of many image transitions, thanks to the extension VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION
    // we can go ahead only with memory barriers.
    // barrier.srcStageMask = vk::PipelineStageFlagBits2::eAllCommands;
    // barrier.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands;
    // barrier.srcAccessMask = vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead;
    // barrier.dstAccessMask = vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead;

    cmd.pipelineBarrier2(depInfo);

    // Copy draw to swapchain
    utils::copy_image(cmd,
                      I->imageDraw.image,
                      I->swapchainImages[swapchainImageIndex],
                      vk::Extent2D{I->imageDraw.extent.width, I->imageDraw.extent.height},
                      I->swapchainExtent);

    cmd.pipelineBarrier2(depInfo);

    draw_imgui(cmd, I->swapchainImageViews[swapchainImageIndex]);

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
    // std::cout << "submit" << std::endl;
    I->graphicsQueue.submit2(submitInfo, nullptr);
    // I->graphicsQueue.submit2(submitInfo, frameFence);

    // prepare present
    // this will put the image we just rendered to into the visible window.
    // we want to wait on the submitSemaphore for that,
    // as its necessary that drawing commands have finished before the image is displayed to the user
    // We signal the frame fence in order for the next execution to now when it's safe to start
    vk::SwapchainPresentFenceInfoKHR swapchainPresentInfo{};
    swapchainPresentInfo.setFences(frameFence);
    swapchainPresentInfo.setSwapchainCount(1);
    vk::PresentInfoKHR presentInfo{};
    presentInfo.setSwapchains(I->swapchain);
    presentInfo.setWaitSemaphores(submitSemaphore);
    presentInfo.setImageIndices(swapchainImageIndex);
    presentInfo.setPNext(&swapchainPresentInfo);

    // std::cout << "present" << std::endl;
    VK_CHECK_RES(I->graphicsQueue.presentKHR(presentInfo));

    I->graphicsQueue.waitIdle();

    frameNumber = (frameNumber + 1) % I->frameOverlap;
}

void Engine::draw_meshes(const vk::CommandBuffer &cmd)
{
    vk::RenderingAttachmentInfo colorAttachmentInfo{};
    colorAttachmentInfo.setImageView(I->imageDraw.imageView);
    colorAttachmentInfo.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal);
    colorAttachmentInfo.setLoadOp(vk::AttachmentLoadOp::eLoad);
    colorAttachmentInfo.setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingAttachmentInfo depthAttachmentInfo{};
    depthAttachmentInfo.setImageView(I->imageDepth.imageView);
    depthAttachmentInfo.setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal);
    depthAttachmentInfo.setLoadOp(vk::AttachmentLoadOp::eClear);
    depthAttachmentInfo.setStoreOp(vk::AttachmentStoreOp::eStore);
    depthAttachmentInfo.setClearValue(vk::ClearValue{vk::ClearDepthStencilValue{1.f}});

    vk::RenderingInfo renderInfo{};
    renderInfo.setColorAttachments(colorAttachmentInfo);
    renderInfo.setPDepthAttachment(&depthAttachmentInfo);
    renderInfo.setLayerCount(1);
    renderInfo.setRenderArea(vk::Rect2D{vk::Offset2D{0, 0}, I->swapchainExtent});

    //set dynamic viewport and scissor
    vk::Viewport viewport{};
    viewport.setX(0.f);
    viewport.setY(0.f);
    viewport.setWidth(I->swapchainExtent.width);
    viewport.setHeight(I->swapchainExtent.height);
    viewport.setMinDepth(0.f);
    viewport.setMaxDepth(1.f);

    vk::Rect2D scissor{};
    scissor.setExtent(vk::Extent2D{I->swapchainExtent.width, I->swapchainExtent.height});
    scissor.setOffset(vk::Offset2D{0, 0});

    try {
        cmd.beginRendering(renderInfo);
        cmd.setViewport(0, viewport);
        cmd.setScissor(0, scissor);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, I->simpleMeshGraphicsPipeline.pipeline);

        vk::DescriptorSet descriptorSet = get_current_frame().descriptorSetUAB;

        vk::BindDescriptorSetsInfo descSetsInfo{};
        descSetsInfo.setDescriptorSets(descriptorSet);
        descSetsInfo.setStageFlags(vk::ShaderStageFlagBits::eVertex);
        descSetsInfo.setLayout(I->simpleMeshGraphicsPipeline.pipelineLayout);
        cmd.bindDescriptorSets2(descSetsInfo);

        I->camera.update();

        for (int objId = 0; objId < I->models.size(); objId++) {
            glm::mat4 mvpMatrix = I->camera.projMatrix * I->camera.viewMatrix
                                  * I->models[objId]->modelMatrix;

            UniformData uboData;
            uboData.worldMatrix = mvpMatrix;

            utils::map_to_buffer(I->models[objId]->uniformBuffer, &uboData);
        }

        for (int objId = 0; objId < I->models.size(); objId++) {
            MeshPush pushConstants;
            pushConstants.objId = objId;
            cmd.pushConstants(I->simpleMeshGraphicsPipeline.pipelineLayout,
                              vk::ShaderStageFlagBits::eVertex,
                              0,
                              sizeof(MeshPush),
                              &pushConstants);

            cmd.draw(I->models[objId]->surfaces[0].count,
                     1,
                     I->models[objId]->surfaces[0].startIndex,
                     0);

            // USING THE INDEX BUFFER PROPERLY REQUIRES THE FOLLOWING DRAW CALL:
            // cmd.bindIndexBuffer2(I->models[objId]->gpuMesh.meshBuffer.indexBuffer.buffer,
            //                      0,
            //                      I->models[objId]->gpuMesh.meshBuffer.indexBuffer.allocationInfo.size,
            //                      vk::IndexType::eUint32);

            // cmd.drawIndexed(I->models[objId]->gpuMesh.surfaces[0].count,
            //                 1,
            //                 I->models[objId]->gpuMesh.surfaces[0].startIndex,
            //                 0,
            //                 0);
        }
        cmd.endRendering();
    } catch (const std::exception &e) {
        VK_CHECK_EXC(e);
    }
}

void Engine::raytrace(const vk::CommandBuffer &cmd)
{
    cmd.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, I->simpleRtPipeline.pipeline);

    vk::DescriptorSet descriptorSetUniform = get_current_frame().descriptorSetUAB;
    vk::DescriptorSet descriptorSetRt = get_current_frame().descriptorSetRt;
    vk::ImageView imageView = I->imageDraw.imageView;
    vk::AccelerationStructureKHR tlas = I->tlas.AS;

    std::vector<vk::DescriptorSet> descriptorSets = {descriptorSetRt, descriptorSetUniform};
    vk::BindDescriptorSetsInfo bindSetsInfo{};
    bindSetsInfo.setDescriptorSets(descriptorSets);
    bindSetsInfo.setStageFlags(vk::ShaderStageFlagBits::eRaygenKHR
                               | vk::ShaderStageFlagBits::eClosestHitKHR
                               | vk::ShaderStageFlagBits::eMissKHR);
    bindSetsInfo.setLayout(I->simpleRtPipeline.pipelineLayout);
    bindSetsInfo.setFirstSet(0);

    cmd.bindDescriptorSets2(bindSetsInfo);

    // rayPush.clearColor = glm::vec4(0.f, 0.0f, 0.2f, 1.f);
    // rayPush.numObjects = I->models.size();
    // rayPush.lightIntensity = 10.f;
    // rayPush.lightType = 0;
    static uint32_t t = 180;
    const float tf = glm::radians(static_cast<float>(t));
    rayPush.lightPosition = glm::vec3(5.f * std::cosf(tf), -5.f, 7.f + 5.f * std::sinf(tf));
    t = (t + 1) % 360;
    vk::PushConstantsInfo pushInfo{};
    pushInfo.setLayout(I->simpleRtPipeline.pipelineLayout);
    pushInfo.setStageFlags(vk::ShaderStageFlagBits::eRaygenKHR
                           | vk::ShaderStageFlagBits::eClosestHitKHR
                           | vk::ShaderStageFlagBits::eMissKHR);
    pushInfo.setValues<RayPush>(rayPush);
    pushInfo.setOffset(0);
    cmd.pushConstants2(pushInfo);

    I->camera.update();

    for (int objId = 0; objId < I->models.size(); objId++) {
        glm::mat4 mvpMatrix = I->camera.projMatrix * I->camera.viewMatrix
                              * I->models[objId]->modelMatrix;

        UniformData uboData;
        uboData.worldMatrix = mvpMatrix;

        utils::map_to_buffer(uniformBuffers[objId], &uboData);
    }

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
