#pragma once
#include "types.hpp"
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
// #include <fastgltf/glm_element_traits.hpp>
// #include <fastgltf/tools.hpp>

struct HostMeshAsset2
{};

struct Node
{
    // parent pointer must be a weak pointer to avoid circular dependencies
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;

    glm::mat4 localTransform;
    glm::mat4 worldTransform;

    void refreshTransform(const glm::mat4 &parentMatrix)
    {
        worldTransform = parentMatrix * localTransform;
        for (std::shared_ptr<Node> &c : children)
            c->refreshTransform(worldTransform);
    }
};

struct GLTFMaterial
{
    struct MaterialConstants
    {
        glm::vec4 baseColorFactor;
        float metallicFactor;
        float roughnessFactor;
    };

    struct MaterialResources
    {
        ImageData colorImage;
        vk::Sampler colorSampler;
        ImageData metalRoughImage;
        vk::Sampler metalRoughSampler;
        Buffer dataBuffer;
        // uint32_t dataBufferOffset; // I should not need offset if going bindless
    };

    enum struct MaterialPass : uint8_t { MainColor, Transparent, Other };

    MaterialConstants materialConstants;
    MaterialPass materialPass;
    MaterialResources materialResources;
};

struct GeoSurface2
{
    uint32_t startIndex;
    uint32_t count;
    std::shared_ptr<GLTFMaterial> material;
};

struct DeviceMesh
{
    std::string name;

    std::vector<GeoSurface2> surfaces;
    Buffer indexBuffer, vertexBuffer;
};

struct MeshNode : public Node
{
    std::shared_ptr<DeviceMesh> mesh;
};

struct GLTFObj
{
    // storage for all the data on a given gltf file
    std::unordered_map<std::string, std::shared_ptr<DeviceMesh>> meshes;
    std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
    std::unordered_map<std::string, ImageData> images;
    std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

    // nodes that dont have a parent, for iterating through the file in tree order
    std::vector<std::shared_ptr<Node>> topNodes;

    std::vector<vk::Sampler> samplers;

    Buffer materialConstantsBuffer;

    ~GLTFObj() = default;
};

vk::Filter extract_filter(const fastgltf::Filter &filter);

vk::SamplerMipmapMode extract_mipmap_mode(const fastgltf::Filter &filter);

class GLTFLoader2
{
public:
    GLTFLoader2(const vk::Device &device,
                const VmaAllocator &allocator,
                const uint32_t queueFamilyIndex);

    ~GLTFLoader2();

    std::optional<std::shared_ptr<GLTFObj>> load_gltf_asset(const std::filesystem::path &path);
    void create_mesh_buffers(const std::vector<uint32_t> &indices,
                             const std::vector<Vertex> &vertices,
                             std::shared_ptr<DeviceMesh> &mesh);

private:
    const vk::Device &device;
    const VmaAllocator &allocator;
    vk::Queue queue;
    vk::CommandPool gltfCmdPool;
    vk::CommandBuffer gltfCmd;
    vk::Fence gltfFence;
    ImageData checkerboardImage, whiteImage, blackImage, greyImage;
    vk::Sampler samplerLinear, samplerNearest;
    fastgltf::Parser parser{};
};
