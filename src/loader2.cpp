#include "loader2.hpp"
#include "utils.hpp"
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/tools.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <print>

GLTFLoader2::GLTFLoader2(const vk::Device &device,
                         const VmaAllocator &allocator,
                         const uint32_t queueFamilyIndex)
    : device{device}
    , allocator{allocator}
{
    // Create pool
    vk::CommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    commandPoolCreateInfo.setQueueFamilyIndex(queueFamilyIndex);
    gltfCmdPool = device.createCommandPool(commandPoolCreateInfo);

    // Get queue
    vk::DeviceQueueInfo2 queueInfo{};
    queueInfo.setQueueFamilyIndex(queueFamilyIndex);
    queueInfo.setQueueIndex(0); // Why can't I use index 1?
    queue = device.getQueue2(queueInfo);

    // Create command buffer
    vk::CommandBufferAllocateInfo cmdBufferAllocInfo{};
    cmdBufferAllocInfo.setCommandPool(gltfCmdPool);
    cmdBufferAllocInfo.setCommandBufferCount(1);
    cmdBufferAllocInfo.setLevel(vk::CommandBufferLevel::ePrimary);
    gltfCmd = device.allocateCommandBuffers(cmdBufferAllocInfo)[0];

    // Create fence
    vk::FenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);
    gltfFence = device.createFence(fenceCreateInfo);

    // Create default images for default textures
    uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
    uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
    uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
    uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));

    std::array<uint32_t, 16 * 16> pixels; // for 16x16 checkerboard texture
    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
            pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }
    checkerboardImage = utils::create_image(device,
                                            allocator,
                                            gltfCmd,
                                            gltfFence,
                                            queue,
                                            vk::Format::eR8G8B8A8Unorm,
                                            vk::ImageUsageFlagBits::eSampled
                                                | vk::ImageUsageFlagBits::eTransferDst,
                                            vk::Extent3D{16, 16, 1},
                                            pixels.data());

    whiteImage = utils::create_image(device,
                                     allocator,
                                     gltfCmd,
                                     gltfFence,
                                     queue,
                                     vk::Format::eR8G8B8A8Unorm,
                                     vk::ImageUsageFlagBits::eSampled
                                         | vk::ImageUsageFlagBits::eTransferDst,
                                     vk::Extent3D{1, 1, 1},
                                     &white);

    greyImage = utils::create_image(device,
                                    allocator,
                                    gltfCmd,
                                    gltfFence,
                                    queue,
                                    vk::Format::eR8G8B8A8Unorm,
                                    vk::ImageUsageFlagBits::eSampled
                                        | vk::ImageUsageFlagBits::eTransferDst,
                                    vk::Extent3D{1, 1, 1},
                                    &grey);

    blackImage = utils::create_image(device,
                                     allocator,
                                     gltfCmd,
                                     gltfFence,
                                     queue,
                                     vk::Format::eR8G8B8A8Unorm,
                                     vk::ImageUsageFlagBits::eSampled
                                         | vk::ImageUsageFlagBits::eTransferDst,
                                     vk::Extent3D{1, 1, 1},
                                     &black);

    // Default samplers
    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.setMagFilter(vk::Filter::eLinear);
    samplerInfo.setMinFilter(vk::Filter::eLinear);
    samplerLinear = device.createSampler(samplerInfo);

    samplerInfo.setMagFilter(vk::Filter::eNearest);
    samplerInfo.setMinFilter(vk::Filter::eNearest);
    samplerNearest = device.createSampler(samplerInfo);
}

GLTFLoader2::~GLTFLoader2()
{
    vmaDestroyImage(allocator, checkerboardImage.image, checkerboardImage.allocation);
    vmaDestroyImage(allocator, whiteImage.image, whiteImage.allocation);
    vmaDestroyImage(allocator, blackImage.image, blackImage.allocation);
    vmaDestroyImage(allocator, greyImage.image, greyImage.allocation);
    device.destroyImageView(checkerboardImage.imageView);
    device.destroyImageView(whiteImage.imageView);
    device.destroyImageView(blackImage.imageView);
    device.destroyImageView(greyImage.imageView);
    device.destroyFence(gltfFence);
    device.destroyCommandPool(gltfCmdPool);
    device.destroySampler(samplerLinear);
    device.destroySampler(samplerNearest);
}

std::optional<std::shared_ptr<GLTFObj>> GLTFLoader2::load_gltf_asset(
    const std::filesystem::path &path)
{
    // This function already asserts if path exists
    const auto pathAbsolute = std::filesystem::canonical(path).string();
    // so no need to assert here anything
    std::println("Loading GLTF asset from {}", pathAbsolute);
    auto data = fastgltf::GltfDataBuffer::FromPath(path);

    // Load asset and check that there were no issues
    constexpr auto options = fastgltf::Options::DontRequireValidAssetMember
                             | fastgltf::Options::LoadExternalBuffers
                             | fastgltf::Options::LoadExternalImages;
    auto asset = parser.loadGltfBinary(data.get(), path.parent_path(), options);
    // auto val = fastgltf::validate(asset.get());
    if (asset.error() != fastgltf::Error::None /*|| val != fastgltf::Error::None*/) {
        std::println("Error in parsing GLTF asset: {}", fastgltf::getErrorMessage(asset.error()));
        // std::println("Error in parsing GLTF asset: {}", fastgltf::getErrorMessage(val));
        return {};
    }

    // Create the main object which will hold all the gltf data
    std::shared_ptr<GLTFObj> scene = std::make_shared<GLTFObj>();

    // Load samplers
    scene->samplers.reserve(asset->samplers.size());
    for (const fastgltf::Sampler &s : asset->samplers) {
        vk::SamplerCreateInfo samplerCreate{};
        samplerCreate.setMaxLod(vk::LodClampNone);
        samplerCreate.setMinLod(0.f);
        samplerCreate.setMagFilter(extract_filter(s.magFilter.value_or(fastgltf::Filter::Nearest)));
        samplerCreate.setMinFilter(extract_filter(s.minFilter.value_or(fastgltf::Filter::Nearest)));
        samplerCreate.setMipmapMode(
            extract_mipmap_mode(s.minFilter.value_or(fastgltf::Filter::Nearest)));

        vk::Sampler sampler = device.createSampler(samplerCreate);
        scene->samplers.emplace_back(sampler);
    }

    // Temporal arrays for all the objects to use while creating the GLTF data
    std::vector<std::shared_ptr<DeviceMesh>> meshes;
    std::vector<std::shared_ptr<Node>> nodes;
    std::vector<ImageData> images;
    std::vector<std::shared_ptr<GLTFMaterial>> materials;

    // Load textures. NOT YET
    images.reserve(asset->images.size());
    for (const fastgltf::Image &im : asset->images) {
        images.emplace_back(checkerboardImage);
        scene->images[im.name.c_str()] = images.back();
    }

    // MATERIALS
    // Loop over the PBR materials
    materials.reserve(asset->materials.size());
    for (size_t i = 0; i < asset->materials.size(); i++) {
        const fastgltf::Material &m = asset->materials[i];
        std::shared_ptr<GLTFMaterial> matTmp = std::make_shared<GLTFMaterial>();

        // Write constants to temporal material
        matTmp->materialConstants.baseColorFactor.x = m.pbrData.baseColorFactor.x();
        matTmp->materialConstants.baseColorFactor.y = m.pbrData.baseColorFactor.y();
        matTmp->materialConstants.baseColorFactor.z = m.pbrData.baseColorFactor.z();
        matTmp->materialConstants.baseColorFactor.w = m.pbrData.baseColorFactor.w();
        matTmp->materialConstants.metallicFactor = m.pbrData.metallicFactor;
        matTmp->materialConstants.roughnessFactor = m.pbrData.roughnessFactor;

        // Copy them to buffer
        matTmp->materialResources.dataBuffer
            = utils::create_buffer(device,
                                   allocator,
                                   sizeof(GLTFMaterial::MaterialConstants),
                                   vk::BufferUsageFlagBits::eStorageBuffer
                                       | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                   VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                       | VMA_ALLOCATION_CREATE_MAPPED_BIT);

        utils::map_to_buffer(matTmp->materialResources.dataBuffer,
                             allocator,
                             &matTmp->materialConstants);

        // Material type
        matTmp->materialPass = (m.alphaMode == fastgltf::AlphaMode::Blend)
                                   ? GLTFMaterial::MaterialPass::Transparent
                                   : GLTFMaterial::MaterialPass::MainColor;
        // Fill materials[i]:
        matTmp->materialResources.colorImage = whiteImage;
        matTmp->materialResources.colorSampler = samplerLinear;
        matTmp->materialResources.metalRoughImage = whiteImage;
        matTmp->materialResources.metalRoughSampler = samplerLinear;
        // matTmp->materialResources.dataBuffer = scene->materialConstantsBuffer;
        if (m.pbrData.baseColorTexture.has_value()) {
            size_t imgIndex = asset->textures[m.pbrData.baseColorTexture->textureIndex]
                                  .imageIndex.value();
            size_t samplerIndex = asset->textures[m.pbrData.baseColorTexture->textureIndex]
                                      .samplerIndex.value();
            matTmp->materialResources.colorImage = images[imgIndex];
            matTmp->materialResources.colorSampler = scene->samplers[samplerIndex];
        }
        materials.emplace_back(std::move(matTmp));
        scene->materials[m.name.c_str()] = materials.back();
        // WORK OUT THE DESCRIPTORS HERE?
    }
    // MESHES
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;
    meshes.reserve(asset->meshes.size());
    for (const fastgltf::Mesh &m : asset->meshes) {
        std::shared_ptr<DeviceMesh> meshTmp = std::make_shared<DeviceMesh>();
        meshTmp->name = m.name.c_str();

        // Load primitives
        // Do a first pass over the primitives for efficiently reserve memory
        indices.clear();
        vertices.clear();
        size_t indexCount = 0;
        size_t vertexCount = 0;
        for (const fastgltf::Primitive &p : m.primitives) {
            const fastgltf::Accessor &indicesAccessor = asset->accessors[p.indicesAccessor.value()];
            const fastgltf::Accessor &verticesAccessor
                = asset->accessors[p.findAttribute("POSITION")->accessorIndex];
            indexCount += indicesAccessor.count;
            vertexCount += verticesAccessor.count;
        }

        indices.reserve(indexCount);
        vertices.resize(vertexCount);
        // Access the data of each primitive
        size_t initialVtx = 0;
        meshTmp->surfaces.reserve(m.primitives.size());
        for (const fastgltf::Primitive &p : m.primitives) {
            // Load indices
            const fastgltf::Accessor &indicesAccessor = asset->accessors[p.indicesAccessor.value()];

            GeoSurface2 surface;
            surface.startIndex = static_cast<uint32_t>(indices.size());
            surface.count = indicesAccessor.count;

            fastgltf::iterateAccessor<uint32_t>(asset.get(), indicesAccessor, [&](uint32_t index) {
                indices.emplace_back(index + initialVtx);
            });

            // Load vertex positions. No need to check since gltf 2 standard requires POSITION to be
            // always present
            const fastgltf::Accessor &verticesAccessor
                = asset->accessors[p.findAttribute("POSITION")->accessorIndex];
            fastgltf::iterateAccessorWithIndex<glm::vec3>(asset.get(),
                                                          verticesAccessor,
                                                          [&](const glm::vec3 &v, const size_t idx) {
                                                              vertices[initialVtx + idx].position = v;
                                                          });

            // Load per-vertex normals
            const fastgltf::Attribute *normalAttrib = p.findAttribute("NORMAL");
            if (normalAttrib != p.attributes.cend()) {
                const fastgltf::Accessor &normalsAccessor
                    = asset->accessors[normalAttrib->accessorIndex];
                fastgltf::iterateAccessorWithIndex<glm::vec3>(
                    asset.get(), normalsAccessor, [&](const glm::vec3 &n, const size_t idx) {
                        vertices[initialVtx + idx].normal = n;
                    });
            }

            // Load per-vertex UVs
            const fastgltf::Attribute *uvAttrib = p.findAttribute("TEXCOORD_0");
            if (uvAttrib != p.attributes.cend()) {
                const fastgltf::Accessor &uvAccessor = asset->accessors[uvAttrib->accessorIndex];
                fastgltf::iterateAccessorWithIndex<glm::vec2>(
                    asset.get(), uvAccessor, [&](const glm::vec2 &uv, const size_t idx) {
                        vertices[initialVtx + idx].uvX = uv.x;
                        vertices[initialVtx + idx].uvY = uv.y;
                    });
            }

            // Load vertex colors
            const fastgltf::Attribute *colorAttrib = p.findAttribute("COLOR_0");
            if (colorAttrib != p.attributes.cend()) {
                const fastgltf::Accessor &colorAccessor
                    = asset->accessors[colorAttrib->accessorIndex];
                fastgltf::iterateAccessorWithIndex<glm::vec4>(
                    asset.get(), colorAccessor, [&](const glm::vec4 &c, const size_t idx) {
                        vertices[initialVtx + idx].color = c;
                    });
            }

            // Load material by index
            if (p.materialIndex.has_value())
                surface.material = materials[p.materialIndex.value()];
            else
                surface.material = materials[0];

            meshTmp->surfaces.emplace_back(surface);

            initialVtx += verticesAccessor.count;
        }
        // Fill meshTmp index and vertex buffers
        create_mesh_buffers(indices, vertices, meshTmp);
        meshes.emplace_back(std::move(meshTmp));
        scene->meshes[m.name.c_str()] = meshes.back();
    }

    // Load nodes and their meshes
    nodes.reserve(asset->nodes.size());
    // Define the lambda that will load the node and the local transform
    const auto create_nodes = [&](const fastgltf::Node &n, const fastgltf::math::fmat4x4 &m) {
        std::shared_ptr<Node> nodeTmp;

        // If has a mesh, make the Node a MeshNode and load the mesh
        if (n.meshIndex.has_value()) {
            nodeTmp = std::make_shared<MeshNode>();
            static_cast<MeshNode>(*nodeTmp).mesh = meshes[n.meshIndex.value()];
        } else
            nodeTmp = std::make_shared<Node>();

        // Load the local transform
        nodeTmp->localTransform = glm::make_mat4(m.data());

        // Add the node to our structures
        nodes.emplace_back(std::move(nodeTmp));
        scene->nodes[n.name.c_str()] = nodes.back();
    };
    // For now, consider all the scenes as a single one (it usually is the case)
    for (size_t sceneId = 0; sceneId < asset->scenes.size(); sceneId++)
        fastgltf::iterateSceneNodes(asset.get(), sceneId, fastgltf::math::fmat4x4(), create_nodes);

    // Generate the node tree structure
    for (size_t i = 0; i < nodes.size(); i++) {
        const fastgltf::Node &fgltfNode = asset->nodes[i];
        std::shared_ptr<Node> &node = nodes[i];
        node->children.reserve(fgltfNode.children.size());
        for (const size_t c : fgltfNode.children) {
            nodes[c]->parent = node;
            node->children.emplace_back(nodes[c]);
        }
    }

    // Find the top nodes and propagate/fill the worldTransform matrices to all nodes
    for (const std::shared_ptr<Node> &n : nodes) {
        if (n->parent.lock() == nullptr) {
            scene->topNodes.push_back(n);
            n->refreshTransform(glm::mat4(1.f));
        }
    }

    return scene;
}

void GLTFLoader2::create_mesh_buffers(const std::vector<uint32_t> &indices,
                                      const std::vector<Vertex> &vertices,
                                      std::shared_ptr<DeviceMesh> &mesh)
{
    const vk::DeviceSize verticesSize = vertices.size() * sizeof(Vertex);
    const vk::DeviceSize indicesSize = indices.size() * sizeof(uint32_t);

    mesh->vertexBuffer = utils::create_buffer(
        device,
        allocator,
        verticesSize,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress
            | vk::BufferUsageFlagBits::eTransferDst
            | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

    mesh->indexBuffer = utils::create_buffer(
        device,
        allocator,
        indicesSize,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress
            | vk::BufferUsageFlagBits::eTransferDst
            | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

    Buffer stagingBuffer = utils::create_buffer(device,
                                                allocator,
                                                verticesSize + indicesSize,
                                                vk::BufferUsageFlagBits::eTransferSrc,
                                                VMA_MEMORY_USAGE_AUTO,
                                                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                                    | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    utils::map_to_buffer(stagingBuffer, allocator, vertices.data(), verticesSize);
    utils::map_to_buffer(stagingBuffer, allocator, indices.data(), indicesSize, verticesSize);

    utils::cmd_submit(device, queue, gltfFence, gltfCmd, [&](const vk::CommandBuffer &cmd) {
        // Set info structures to copy from staging to vertex & index buffers
        vk::BufferCopy2 vertexCopy{};
        vertexCopy.setSrcOffset(0);
        vertexCopy.setDstOffset(0);
        vertexCopy.setSize(verticesSize);
        vk::CopyBufferInfo2 vertexCopyInfo{};
        vertexCopyInfo.setSrcBuffer(stagingBuffer.buffer);
        vertexCopyInfo.setDstBuffer(mesh->vertexBuffer.buffer);
        vertexCopyInfo.setRegions(vertexCopy);

        vk::BufferCopy2 indexCopy{};
        indexCopy.setSrcOffset(verticesSize);
        indexCopy.setDstOffset(0);
        indexCopy.setSize(indicesSize);
        vk::CopyBufferInfo2 indexCopyInfo{};
        indexCopyInfo.setSrcBuffer(stagingBuffer.buffer);
        indexCopyInfo.setDstBuffer(mesh->indexBuffer.buffer);
        indexCopyInfo.setRegions(indexCopy);

        cmd.copyBuffer2(vertexCopyInfo);
        cmd.copyBuffer2(indexCopyInfo);
    });

    vmaDestroyBuffer(allocator, stagingBuffer.buffer, stagingBuffer.allocation);
}

vk::Filter extract_filter(const fastgltf::Filter &filter)
{
    switch (filter) {
    // nearest samplers
    case fastgltf::Filter::Nearest:
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::NearestMipMapLinear:
        return vk::Filter::eNearest;

    // linear samplers
    case fastgltf::Filter::Linear:
    case fastgltf::Filter::LinearMipMapNearest:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
        return vk::Filter::eLinear;
    }
}

vk::SamplerMipmapMode extract_mipmap_mode(const fastgltf::Filter &filter)
{
    switch (filter) {
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::LinearMipMapNearest:
        return vk::SamplerMipmapMode::eNearest;

    case fastgltf::Filter::NearestMipMapLinear:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
        return vk::SamplerMipmapMode::eLinear;
    }
}
