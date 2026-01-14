#include "lights.hpp"
#include "imgui.h"
#include "utils.hpp"

uint32_t Light::nextId = 0;

void Light::upload(const vk::Device &device, const VmaAllocator &allocator)
{
    if (!ubo.buffer) {
        ubo = utils::create_buffer(device,
                                   allocator,
                                   sizeof(Light::LightData),
                                   vk::BufferUsageFlagBits::eUniformBuffer,
                                   VMA_MEMORY_USAGE_AUTO,
                                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                       | VMA_ALLOCATION_CREATE_MAPPED_BIT);
        utils::copy_to_buffer(ubo, allocator, &lightData);
    }

    this->allocator = allocator;
}

void Light::update()
{
    assert(ubo.buffer && allocator);
    utils::copy_to_buffer(ubo, allocator, &lightData);
}

void Light::destroy()
{
    assert(allocator);
    if (ubo.buffer)
        utils::destroy_buffer(allocator, ubo);
}

void LightsManager::run()
{
    ImGui::Begin("Lights Manager");

    if (ImGui::Button("Add Light")) {
        Light light{};
        light.upload(device, allocator);
        lights.push_back(light);
        lightBuffers.push_back(light.ubo);
    }

    ImGui::Separator();

    // Track which light to remove (if any)
    int lightToRemove = -1;

    for (size_t i = 0; i < lights.size(); i++) {
        ImGui::PushID(lights[i].id());

        const std::string header = "Light " + std::to_string(i);
        if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();

            ImGui::RadioButton("Point light", (int *) &lights[i].lightData.type, LightType::ePoint);
            ImGui::SameLine();
            ImGui::RadioButton("Directional light",
                               (int *) &lights[i].lightData.type,
                               LightType::eDirectional);

            bool update{false};
            update = update
                     || ImGui::DragFloat3("Position/Direction",
                                          (float *) &lights[i].lightData.positionOrDirection,
                                          0.1f,
                                          0.f,
                                          0.f);
            update = update || ImGui::ColorEdit3("Color", (float *) &lights[i].lightData.color);
            update = update
                     || ImGui::InputFloat("Intensity",
                                          &lights[i].lightData.intensity,
                                          0.5f,
                                          5.f,
                                          "%.2f");
            lights[i].lightData.intensity = std::max(lights[i].lightData.intensity, 0.f);

            if (ImGui::Button("Remove"))
                lightToRemove = i;

            if (update)
                lights[i].update();

            ImGui::Unindent();
            ImGui::Spacing();
        }

        ImGui::PopID();
    }

    if (lightToRemove >= 0) {
        lights[lightToRemove].destroy();
        lights.erase(std::next(lights.begin(), lightToRemove));
        lightBuffers.erase(std::next(lightBuffers.begin(), lightToRemove));
    }

    ImGui::End();
}
