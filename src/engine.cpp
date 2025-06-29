#ifndef USE_CXX20_MODULES
#else
import vulkan_hpp;
#include <vulkan/vulkan_hpp_macros.hpp>
#endif

#include "engine.hpp"
#include "types.hpp"
#include "utils.hpp"
#include <imgui/imgui_impl_sdl3.h>
#include <imgui/imgui_impl_vulkan.h>
#include <thread>

Engine::Engine()
{
    I = std::make_unique<Init>();
}

Engine::~Engine()
{
    I->clean();
}

void Engine::destroy_buffer(const Buffer &buffer)
{
    vmaDestroyBuffer(I->allocator, buffer.buffer, buffer.allocation);
}


void Engine::run()
{
    SDL_Event e;
    bool quit = false;

    camera.setProjMatrix(glm::radians(70.f),
                         static_cast<float>(I->swapchainExtent.width),
                         static_cast<float>(I->swapchainExtent.height),
                         0.01f,
                         100.f);
    const float dx = 0.5f;
    const float dt = glm::radians(2.f);
    I->models[2]->position = glm::vec3(0.f, 0.f, 7.f);
    I->models[2]->updateModelMatrix();

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
                    camera.forward(dx);
                if (keyStates[SDL_SCANCODE_S])
                    camera.backwards(dx);
                if (keyStates[SDL_SCANCODE_A])
                    camera.left(dx);
                if (keyStates[SDL_SCANCODE_D])
                    camera.right(dx);
                if (keyStates[SDL_SCANCODE_Q])
                    camera.down(dx);
                if (keyStates[SDL_SCANCODE_E])
                    camera.up(dx);
                if (keyStates[SDL_SCANCODE_UP])
                    camera.lookUp(dt);
                if (keyStates[SDL_SCANCODE_DOWN])
                    camera.lookDown(dt);
                if (keyStates[SDL_SCANCODE_LEFT])
                    camera.lookLeft(dt);
                if (keyStates[SDL_SCANCODE_RIGHT])
                    camera.lookRight(dt);
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
            ComputePipelineData &currentPipeline
                = I->computePipelines[currentBackgroundPipelineIndex];
            const std::string text = "Selected background compute shader: " + currentPipeline.name;
            ImGui::Text("%s", text.c_str());
            ImGui::SliderInt("Shader Index: ",
                             &currentBackgroundPipelineIndex,
                             0,
                             I->computePipelines.size() - 1);
            glm::vec4 *colorUp = (glm::vec4 *) I->computePipelines[0].pushData;
            glm::vec4 *colorDown = (glm::vec4 *) ((char *) I->computePipelines[0].pushData
                                                  + sizeof(glm::vec4));
            ImGui::InputFloat4("[ColorGradient] colorUp", (float *) colorUp);
            ImGui::InputFloat4("[ColorGradient] colorDown", (float *) colorDown);
            ImGui::InputFloat4("[Sky] colorW", (float *) I->computePipelines[1].pushData);
        }
        ImGui::End();
        // Imgui UI to test
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
    auto resWaitFences = I->device.waitForFences(get_current_frame().renderFence,
                                                 vk::True,
                                                 FENCE_TIMEOUT);
    VK_CHECK_RES(resWaitFences);
    I->device.resetFences(get_current_frame().renderFence);

    // Request image from the swapchain
    vk::AcquireNextImageInfoKHR acquireImageInfo{};
    acquireImageInfo.setSwapchain(I->swapchain);
    acquireImageInfo.setSemaphore(get_current_frame().swapchainSemaphore);
    acquireImageInfo.setFence(nullptr);
    acquireImageInfo.setTimeout(FENCE_TIMEOUT);
    acquireImageInfo.setDeviceMask(1); // First and only device in the group

    auto [resNextImage, swapchainImageIndex] = I->device.acquireNextImage2KHR(acquireImageInfo);
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
                            I->imageDraw.image,
                            vk::ImageLayout::eUndefined,
                            vk::ImageLayout::eGeneral);
    // Draw into the draw image
    change_background(cmd);

    utils::transition_image(cmd,
                            I->imageDraw.image,
                            vk::ImageLayout::eGeneral,
                            vk::ImageLayout::eColorAttachmentOptimal);

    utils::transition_image(cmd,
                            I->imageDepth.image,
                            vk::ImageLayout::eUndefined,
                            vk::ImageLayout::eDepthAttachmentOptimal);

    draw_meshes(cmd);

    // Transition the draw image to be sent and the swapchain image to receive it
    utils::transition_image(cmd,
                            I->imageDraw.image,
                            vk::ImageLayout::eColorAttachmentOptimal,
                            vk::ImageLayout::eTransferSrcOptimal);
    utils::transition_image(cmd,
                            I->swapchainImages[swapchainImageIndex],
                            vk::ImageLayout::eUndefined,
                            vk::ImageLayout::eTransferDstOptimal);

    // Copy draw to swapchain
    utils::copy_image(cmd,
                      I->imageDraw.image,
                      I->swapchainImages[swapchainImageIndex],
                      vk::Extent2D{I->imageDraw.extent.width, I->imageDraw.extent.height},
                      I->swapchainExtent);

    utils::transition_image(cmd,
                            I->swapchainImages[swapchainImageIndex],
                            vk::ImageLayout::eTransferDstOptimal,
                            vk::ImageLayout::eColorAttachmentOptimal);

    draw_imgui(cmd, I->swapchainImageViews[swapchainImageIndex]);

    utils::transition_image(cmd,
                            I->swapchainImages[swapchainImageIndex],
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
    semaphoreWaitInfo.setStageMask(vk::PipelineStageFlagBits2::eAllGraphics);
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
    I->graphicsQueue.submit2(submitInfo, get_current_frame().renderFence);

    //prepare present
    // this will put the image we just rendered to into the visible window.
    // we want to wait on the _renderSemaphore for that,
    // as its necessary that drawing commands have finished before the image is displayed to the user
    vk::PresentInfoKHR presentInfo{};
    presentInfo.setSwapchains(I->swapchain);
    presentInfo.setWaitSemaphores(get_current_frame().renderSemaphore);
    presentInfo.setImageIndices(swapchainImageIndex);

    auto resQueuePresent = I->graphicsQueue.presentKHR(&presentInfo);
    VK_CHECK_RES(resQueuePresent);

    frameNumber++;
}

void Engine::change_background(const vk::CommandBuffer &cmd)
{
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute,
                     I->computePipelines[currentBackgroundPipelineIndex].pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                           I->computePipelines[currentBackgroundPipelineIndex].pipelineLayout,
                           0,
                           I->drawImageDescriptors,
                           nullptr);
    cmd.pushConstants(I->computePipelines[currentBackgroundPipelineIndex].pipelineLayout,
                      vk::ShaderStageFlagBits::eCompute,
                      0,
                      I->computePipelines[currentBackgroundPipelineIndex].pushDataSize,
                      I->computePipelines[currentBackgroundPipelineIndex].pushData);
    cmd.dispatch(std::ceil((float) I->imageDraw.extent.width / 16.f),
                 std::ceil((float) I->imageDraw.extent.height / 16.f),
                 1);
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

        int objId = 2;
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, I->simpleMeshGraphicsPipeline.pipeline);

        // models[2]->updateModelMatrix();
        camera.update();
        glm::mat4 mvpMatrix = camera.projMatrix * camera.viewMatrix * I->models[2]->modelMatrix;

        MeshPush pushConstants;
        pushConstants.worldMatrix = mvpMatrix;
        pushConstants.vertexBufferAddress = I->models[objId]->gpuMesh.meshBuffer.vertexBufferAddress;
        cmd.pushConstants(I->simpleMeshGraphicsPipeline.pipelineLayout,
                          vk::ShaderStageFlagBits::eVertex,
                          0,
                          sizeof(MeshPush),
                          &pushConstants);
        cmd.bindIndexBuffer(I->models[objId]->gpuMesh.meshBuffer.indexBuffer.buffer,
                            0,
                            vk::IndexType::eUint32);
        cmd.drawIndexed(I->models[objId]->gpuMesh.surfaces[0].count,
                        1,
                        I->models[objId]->gpuMesh.surfaces[0].startIndex,
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
    renderInfo.setRenderArea(vk::Rect2D{vk::Offset2D{0, 0}, I->swapchainExtent});

    cmd.beginRendering(renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    cmd.endRendering();
}
