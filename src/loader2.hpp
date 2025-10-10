#pragma once
#include "types.hpp"
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
// #include <fastgltf/glm_element_traits.hpp>
// #include <fastgltf/tools.hpp>

struct HostMeshAsset2
{};

struct DeviceMeshAsset2
{};

struct Node
{};

struct Material2
{};

struct GLTFObj
{
    // storage for all the data on a given gltf file
    std::unordered_map<std::string, std::shared_ptr<DeviceMeshAsset2>> meshes;
    std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
    std::unordered_map<std::string, ImageData> images;
    std::unordered_map<std::string, std::shared_ptr<Material2>> materials;

    // nodes that dont have a parent, for iterating through the file in tree order
    std::vector<std::shared_ptr<Node>> topNodes;

    std::vector<vk::Sampler> samplers;

    Buffer materialDataBuffer;

    ~GLTFObj() = default;
};

vk::Filter extract_filter(const fastgltf::Filter &filter);

vk::SamplerMipmapMode extract_mipmap_mode(const fastgltf::Filter &filter);

class GLTFLoader2
{
public:
    GLTFLoader2(const vk::Device &device);

    ~GLTFLoader2() = default;

    std::optional<std::shared_ptr<GLTFObj>> load_gltf_asset(const std::filesystem::path &path);

private:
    const vk::Device &device;
    // const ImageData &checkerboardImage;
    fastgltf::Parser parser{};
};
