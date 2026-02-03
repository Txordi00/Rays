#include "lights.hpp"
#include "imgui.h"
#include "utils.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>

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
    LightData uploadData{lightData};
    assert(ubo.buffer && allocator);
    if (lightData.type == LightType::eDirectional
        && glm::length2(lightData.positionOrDirection) > 0.f)
        uploadData.positionOrDirection = glm::normalize(lightData.positionOrDirection);
    utils::copy_to_buffer(ubo, allocator, &uploadData);
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

    if (ImGui::Button("Add Light") && lights.size() < static_cast<size_t>(MAX_LIGHTS)) {
        // positionOrDirection = {0.f, 0.f, 0.f};
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
            bool update{false}, resetPositionOrDirection{false};

            update = update
                     || ImGui::RadioButton("Point light",
                                           (int *) &lights[i].lightData.type,
                                           LightType::ePoint);
            ImGui::SameLine();
            update = update
                     || ImGui::RadioButton("Directional light",
                                           (int *) &lights[i].lightData.type,
                                           LightType::eDirectional);
            resetPositionOrDirection = update;

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

            if (resetPositionOrDirection) {
                Light::LightData defaultLightData{};
                lights[i].lightData.positionOrDirection = defaultLightData.positionOrDirection;
                lights[i].lightData.intensity = defaultLightData.intensity;
            }
            if (update) {
                lights[i].update();
                // std::println("Update light {}", i);
            }

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
