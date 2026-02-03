#pragma once
#ifndef USE_CXX20_MODULES
#include <vulkan/vulkan.hpp>
#else
import vulkan;
#endif

#include "types.hpp"
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>

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
        int colorImageIndex{-1};
        int colorSamplerIndex{-1};
        int materialImageIndex{-1};
        int materialSamplerIndex{-1};
        int normalMapIndex{-1};
        int normalSamplerIndex{-1};
        Buffer dataBuffer;
    };

    enum struct MaterialPass : uint8_t { MainColor, Transparent, Other };

    // Having the buffer in materialResources, the material constants are probably not needed here anymore
    // MaterialConstants materialConstants;
    MaterialPass materialPass;
    MaterialResources materialResources;
};

struct Surface
{
    uint32_t startIndex;
    uint32_t count;
    uint32_t bufferIndex;
    std::shared_ptr<GLTFMaterial> material;
};

struct Mesh
{
    std::string name;

    std::vector<Surface> surfaces;
    std::shared_ptr<Buffer> indexBuffer, vertexBuffer;
};

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
    virtual ~Node() = default;
};

struct SurfaceStorage
{
    vk::DeviceAddress indexBufferAddress;
    vk::DeviceAddress vertexBufferAddress;
    vk::DeviceAddress materialConstantsBufferAddress;
    int colorSamplerIndex{-1};
    int colorImageIndex{-1};
    int materialSamplerIndex{-1};
    int materialImageIndex{-1};
    int normalMapIndex{-1};
    int normalSamplerIndex{-1};
    uint32_t startIndex{0};
    uint32_t count{0};
};

struct MeshNode : Node
{
    std::shared_ptr<Mesh> mesh;
};

struct GLTFObj
{
    GLTFObj() = default;
    ~GLTFObj() = default;

    // storage for all the data on a given gltf file
    std::unordered_multimap<std::string, std::shared_ptr<Mesh>> meshes;
    std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
    std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

    // nodes that dont have a parent, for iterating through the file in tree order
    std::vector<std::shared_ptr<Node>> topNodes;
    std::vector<std::shared_ptr<MeshNode>> meshNodes;

    std::vector<Buffer> surfaceUniformBuffers;

    size_t surfaceCount{0};

    std::vector<vk::Sampler> samplers;
    std::vector<ImageData> images;

    void destroy(const vk::Device &device, const VmaAllocator &allocator);

    std::vector<std::shared_ptr<Buffer>> bufferQueue;
    std::vector<vk::Sampler> samplerQueue;
    std::vector<ImageData> imageQueue;
};

vk::Filter extract_filter(const fastgltf::Filter &filter);

vk::SamplerMipmapMode extract_mipmap_mode(const fastgltf::Filter &filter);

class GLTFLoader
{
public:
    GLTFLoader(const vk::Device &device,
               const VmaAllocator &allocator,
               const uint32_t queueFamilyIndex);

    ~GLTFLoader() = default;

    void destroy();

    std::optional<std::shared_ptr<GLTFObj>> load_gltf_asset(const std::filesystem::path &path);

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

    void load_samplers(const fastgltf::Asset &asset, std::shared_ptr<GLTFObj> &scene);

    void load_images(const fastgltf::Asset &asset, std::shared_ptr<GLTFObj> &scene);

    std::optional<ImageData> load_image(const fastgltf::Asset &asset,
                                        const fastgltf::Image &fgltfImage);

    void load_materials(const fastgltf::Asset &asset,
                        std::shared_ptr<GLTFObj> &scene,
                        std::vector<std::shared_ptr<GLTFMaterial>> &vMaterials);

    void load_meshes(const fastgltf::Asset &asset,
                     const std::vector<std::shared_ptr<GLTFMaterial>> &materials,
                     std::shared_ptr<GLTFObj> &scene,
                     std::vector<std::shared_ptr<Mesh>> &meshes);

    Buffer create_surface_buffer(const std::shared_ptr<Mesh> &mesh, const Surface &surface);

    // We will need to modify the meshes in order to accomodate each surface id.
    void load_nodes(const fastgltf::Asset &asset,
                    std::vector<std::shared_ptr<Mesh>> &meshes,
                    std::shared_ptr<GLTFObj> &scene,
                    std::vector<std::shared_ptr<Node>> &nodes);

    void create_mesh_buffers(const std::vector<uint32_t> &indices,
                             const std::vector<Vertex> &vertices,
                             std::shared_ptr<Mesh> &mesh);
};
