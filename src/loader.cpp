#include "loader.hpp"
#include "utils.hpp"
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/tools.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <print>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <chrono>

GLTFLoader::GLTFLoader(const vk::Device &device,
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
    samplerQueue.push_back(samplerLinear);

    samplerInfo.setMagFilter(vk::Filter::eNearest);
    samplerInfo.setMinFilter(vk::Filter::eNearest);
    samplerNearest = device.createSampler(samplerInfo);
    samplerQueue.push_back(samplerNearest);
}

void GLTFLoader::destroy()
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
    for (const vk::Sampler &s : samplerQueue)
        device.destroySampler(s);
    for (const ImageData &im : imageQueue) {
        vmaDestroyImage(allocator, im.image, im.allocation);
        device.destroyImageView(im.imageView);
    }

    destroy_buffers();
}

std::optional<std::shared_ptr<GLTFObj>> GLTFLoader::load_gltf_asset(const std::filesystem::path &path)
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

    // Temporal arrays for all the objects to use while creating the GLTF data
    std::vector<std::shared_ptr<Mesh>> meshes;
    std::vector<std::shared_ptr<Node>> nodes;
    std::vector<ImageData> images;
    std::vector<std::shared_ptr<GLTFMaterial>> materials;

    // Load samplers
    load_samplers(asset.get(), scene);

    // Load textures. NOT YET
    load_images(asset.get(), scene, images);

    // MATERIALS
    // Loop over the PBR materials
    load_materials(asset.get(), images, scene, materials);

    // MESHES
    load_meshes(asset.get(), materials, scene, meshes);

    // Load nodes and their meshes
    load_nodes(asset.get(), meshes, scene, nodes);

    return scene;
}

void GLTFLoader::load_samplers(const fastgltf::Asset &asset, std::shared_ptr<GLTFObj> &scene)
{
    scene->samplers.reserve(asset.samplers.size());
    samplerQueue.reserve(samplerQueue.size() + asset.samplers.size());
    for (const fastgltf::Sampler &s : asset.samplers) {
        vk::SamplerCreateInfo samplerCreate{};
        samplerCreate.setMaxLod(vk::LodClampNone);
        samplerCreate.setMinLod(0.f);
        samplerCreate.setMagFilter(extract_filter(s.magFilter.value_or(fastgltf::Filter::Nearest)));
        samplerCreate.setMinFilter(extract_filter(s.minFilter.value_or(fastgltf::Filter::Nearest)));
        samplerCreate.setMipmapMode(
            extract_mipmap_mode(s.minFilter.value_or(fastgltf::Filter::Nearest)));

        vk::Sampler sampler = device.createSampler(samplerCreate);
        scene->samplers.emplace_back(sampler);
        samplerQueue.emplace_back(sampler);
    }
}

// VERY SLOW
void GLTFLoader::load_images(const fastgltf::Asset &asset,
                             std::shared_ptr<GLTFObj> &scene,
                             std::vector<ImageData> &images)
{
    images.reserve(asset.images.size());
    imageQueue.reserve(imageQueue.size() + asset.images.size());
    // const auto start = std::chrono::system_clock::now();
    for (const fastgltf::Image &im : asset.images) {
        const auto image = load_image(asset, im);
        if (image.has_value()) {
            images.emplace_back(image.value());
            imageQueue.emplace_back(image.value());
        } else {
            std::println("Load image error. Emplacing default image.");
            images.emplace_back(checkerboardImage);
        }
        scene->images[im.name.c_str()] = images.back();
    }
    // const auto end = std::chrono::system_clock::now();
    // std::println("Duration: {}",
    //              std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
}

// stbi_load_from_memory IS VERY SLOW
std::optional<ImageData> GLTFLoader::load_image(const fastgltf::Asset &asset,
                                                const fastgltf::Image &fgltfImage)
{
    ImageData image{};
    int w, h, nChannels;

    const auto create_image_from_data = [&](unsigned char *imData) {
        // std::println("image creation");
        if (imData) {
            vk::Extent3D imSize{};
            imSize.setWidth(w);
            imSize.setHeight(h);
            imSize.setDepth(1);

            image = utils::create_image(device,
                                        allocator,
                                        gltfCmd,
                                        gltfFence,
                                        queue,
                                        vk::Format::eR8G8B8A8Unorm,
                                        vk::ImageUsageFlagBits::eSampled,
                                        imSize,
                                        imData);
            std::println("Image created.");

            stbi_image_free(imData);
        }
    };

    const auto read_from_uri = [&](const fastgltf::sources::URI &filePath) {
        assert(filePath.fileByteOffset == 0); // Stbi doesn't support files with offset.
        assert(filePath.uri.isLocalPath());   // We're only capable of loading local files.
        // std::println("uri");

        const std::filesystem::path fsPath = filePath.uri.fspath();
        unsigned char *imData = stbi_load(fsPath.c_str(), &w, &h, &nChannels, 4);
        create_image_from_data(imData);
    };
    const auto read_from_vector = [&](const fastgltf::sources::Vector &vector,
                                      const size_t bufferViewOffset = 0) {
        unsigned char *imData = stbi_load_from_memory(reinterpret_cast<const stbi_uc *>(
                                                          vector.bytes.data() + bufferViewOffset),
                                                      static_cast<int>(vector.bytes.size()),
                                                      &w,
                                                      &h,
                                                      &nChannels,
                                                      4);
        // std::println("vector");
        create_image_from_data(imData);
    };
    const auto read_from_array = [&](const fastgltf::sources::Array &array,
                                     const size_t bufferViewOffset = 0) {
        unsigned char *imData = stbi_load_from_memory(reinterpret_cast<const stbi_uc *>(
                                                          array.bytes.data() + bufferViewOffset),
                                                      static_cast<int>(array.bytes.size()),
                                                      &w,
                                                      &h,
                                                      &nChannels,
                                                      4);
        // std::println("array");

        create_image_from_data(imData);
    };
    const auto read_from_buffer_view = [&](const fastgltf::sources::BufferView &view) {
        const auto &bufferView = asset.bufferViews[view.bufferViewIndex];
        const auto &buffer = asset.buffers[bufferView.bufferIndex];
        const size_t offset = bufferView.byteOffset;
        // std::println("bufferview");

        // Nested visit
        std::visit(fastgltf::visitor{// We only care about VectorWithMime here, because we
                                     // specify LoadExternalBuffers, meaning all buffers
                                     // are already loaded into a vector.
                                     [](auto &arg) {
                                         std::println("Image read error: Unknown source");
                                     },
                                     [&](const fastgltf::sources::Vector &vector) {
                                         read_from_vector(vector, offset);
                                     },
                                     [&](const fastgltf::sources::Array &array) {
                                         read_from_array(array, offset);
                                     }},
                   buffer.data);
    };
    const auto read_from_byte_view = [&](const fastgltf::sources::ByteView &byteView) {
        unsigned char *imData = stbi_load_from_memory(reinterpret_cast<const stbi_uc *>(
                                                          byteView.bytes.data()),
                                                      static_cast<int>(byteView.bytes.size()),
                                                      &w,
                                                      &h,
                                                      &nChannels,
                                                      4);
        // std::println("byteview");
        create_image_from_data(imData);
    };

    fastgltf::visitor visitor{[](const auto &arg) {
                                  std::println("Image Read error: Image in unknown mode.");
                              },
                              [&](const fastgltf::sources::CustomBuffer &cBuffer) {
                                  std::println("Image Read error: Image in custom Buffer mode.");
                              },
                              [&](const fastgltf::sources::Fallback &fallback) {
                                  std::println("Image Read error: Image in fallback mode.");
                              },
                              read_from_uri,
                              read_from_vector,
                              read_from_array,
                              read_from_buffer_view,
                              read_from_byte_view};
    std::visit(visitor, fgltfImage.data);
    if (image.image)
        return image;
    else
        return {};
}

void GLTFLoader::load_materials(const fastgltf::Asset &asset,
                                const std::vector<ImageData> &images,
                                std::shared_ptr<GLTFObj> &scene,
                                std::vector<std::shared_ptr<GLTFMaterial>> &vMaterials)
{
    vMaterials.reserve(asset.materials.size());
    bufferQueue.reserve(bufferQueue.size() + asset.materials.size());
    for (size_t i = 0; i < asset.materials.size(); i++) {
        const fastgltf::Material &m = asset.materials[i];
        std::shared_ptr<GLTFMaterial> matTmp = std::make_shared<GLTFMaterial>();

        // Write constants to material buffer
        GLTFMaterial::MaterialConstants materialConstants;
        materialConstants.baseColorFactor.x = m.pbrData.baseColorFactor.x();
        materialConstants.baseColorFactor.y = m.pbrData.baseColorFactor.y();
        materialConstants.baseColorFactor.z = m.pbrData.baseColorFactor.z();
        materialConstants.baseColorFactor.w = m.pbrData.baseColorFactor.w();
        materialConstants.metallicFactor = m.pbrData.metallicFactor;
        materialConstants.roughnessFactor = m.pbrData.roughnessFactor;

        std::shared_ptr<Buffer> materialConstantsBuffer = std::make_shared<Buffer>();
        *materialConstantsBuffer
            = utils::create_buffer(device,
                                   allocator,
                                   sizeof(GLTFMaterial::MaterialConstants),
                                   vk::BufferUsageFlagBits::eStorageBuffer
                                       | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                   VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
        matTmp->materialResources.dataBuffer = *materialConstantsBuffer;
        bufferQueue.emplace_back(materialConstantsBuffer);
        utils::copy_to_buffer(matTmp->materialResources.dataBuffer, allocator, &materialConstants);

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
            size_t imgIndex = asset.textures[m.pbrData.baseColorTexture->textureIndex]
                                  .imageIndex.value();
            size_t samplerIndex = asset.textures[m.pbrData.baseColorTexture->textureIndex]
                                      .samplerIndex.value();
            matTmp->materialResources.colorImage = images[imgIndex];
            matTmp->materialResources.colorSampler = scene->samplers[samplerIndex];
        }
        vMaterials.emplace_back(std::move(matTmp));
        scene->materials[m.name.c_str()] = vMaterials.back();
        // WORK OUT THE DESCRIPTORS HERE?
    }
}

void GLTFLoader::load_meshes(const fastgltf::Asset &asset,
                             const std::vector<std::shared_ptr<GLTFMaterial>> &materials,
                             std::shared_ptr<GLTFObj> &scene,
                             std::vector<std::shared_ptr<Mesh>> &meshes)
{
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;
    meshes.reserve(asset.meshes.size());
    for (const fastgltf::Mesh &m : asset.meshes) {
        std::shared_ptr<Mesh> meshTmp = std::make_shared<Mesh>();
        meshTmp->name = m.name.c_str();

        // If we already filled the mesh buffers of a given object, do not do it again
        const bool meshBuffersExist = scene->meshes.contains(meshTmp->name);

        // Load primitives
        // Do a first pass over the primitives for efficiently reserve memory
        indices.clear();
        vertices.clear();
        size_t indexCount = 0;
        size_t vertexCount = 0;
        // Add indices and vertices only if we have not visited that mesh yet
        if (!meshBuffersExist) {
            for (const fastgltf::Primitive &p : m.primitives) {
                const fastgltf::Accessor &indicesAccessor
                    = asset.accessors[p.indicesAccessor.value()];
                const fastgltf::Accessor &verticesAccessor
                    = asset.accessors[p.findAttribute("POSITION")->accessorIndex];
                indexCount += indicesAccessor.count;
                vertexCount += verticesAccessor.count;
            }
            indices.reserve(indexCount);
            vertices.resize(vertexCount);
        }

        // Access the data of each primitive
        size_t initialVtx = 0;
        meshTmp->surfaces.reserve(m.primitives.size());
        for (const fastgltf::Primitive &p : m.primitives) {
            // We are going to fill the surface and the i&v buffers in this loop
            Surface surface;

            // Load material by index
            if (p.materialIndex.has_value())
                surface.material = materials[p.materialIndex.value()];
            else
                surface.material = materials[0];

            // Reuse the previous
            if (meshBuffersExist) {
                const auto &sameMesh = scene->meshes.find(meshTmp->name)->second;
                surface.startIndex = sameMesh->surfaces[0].startIndex;
                surface.count = sameMesh->surfaces[0].count;
            } else { // If not visited this mesh earlier, add the indices & vertices
                // Load indices
                const fastgltf::Accessor &indicesAccessor
                    = asset.accessors[p.indicesAccessor.value()];

                surface.startIndex = static_cast<uint32_t>(indices.size());
                surface.count = static_cast<uint32_t>(indicesAccessor.count);

                fastgltf::iterateAccessor<uint32_t>(asset, indicesAccessor, [&](uint32_t index) {
                    indices.emplace_back(index + initialVtx);
                });

                // Load vertex positions. No need to check since gltf 2 standard requires POSITION to be
                // always present
                const fastgltf::Accessor &verticesAccessor
                    = asset.accessors[p.findAttribute("POSITION")->accessorIndex];
                fastgltf::iterateAccessorWithIndex<glm::vec3>(
                    asset, verticesAccessor, [&](const glm::vec3 &v, const size_t idx) {
                        vertices[initialVtx + idx].position = v;
                    });

                // Load per-vertex normals
                const fastgltf::Attribute *normalAttrib = p.findAttribute("NORMAL");
                if (normalAttrib != p.attributes.cend()) {
                    const fastgltf::Accessor &normalsAccessor
                        = asset.accessors[normalAttrib->accessorIndex];
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(
                        asset, normalsAccessor, [&](const glm::vec3 &n, const size_t idx) {
                            vertices[initialVtx + idx].normal = n;
                        });
                }

                // Load per-vertex UVs
                const fastgltf::Attribute *uvAttrib = p.findAttribute("TEXCOORD_0");
                if (uvAttrib != p.attributes.cend()) {
                    const fastgltf::Accessor &uvAccessor = asset.accessors[uvAttrib->accessorIndex];
                    fastgltf::iterateAccessorWithIndex<glm::vec2>(
                        asset, uvAccessor, [&](const glm::vec2 &uv, const size_t idx) {
                            vertices[initialVtx + idx].uvX = uv.x;
                            vertices[initialVtx + idx].uvY = uv.y;
                        });
                }

                // Load vertex colors
                const fastgltf::Attribute *colorAttrib = p.findAttribute("COLOR_0");
                if (colorAttrib != p.attributes.cend()) {
                    const fastgltf::Accessor &colorAccessor
                        = asset.accessors[colorAttrib->accessorIndex];
                    fastgltf::iterateAccessorWithIndex<glm::vec4>(
                        asset, colorAccessor, [&](const glm::vec4 &c, const size_t idx) {
                            vertices[initialVtx + idx].color = c;
                        });
                }
                initialVtx += verticesAccessor.count;
            }
            meshTmp->surfaces.emplace_back(surface);
        }
        // Fill meshTmp index and vertex buffers
        if (!meshBuffersExist)
            create_mesh_buffers(indices, vertices, meshTmp);
        else {
            const auto &sameMesh = scene->meshes.find(meshTmp->name)->second;
            meshTmp->indexBuffer = sameMesh->indexBuffer;
            meshTmp->vertexBuffer = sameMesh->vertexBuffer;
        }
        meshes.emplace_back(std::move(meshTmp));
        scene->meshes.insert({m.name.c_str(), meshes.back()});
    }
}

void GLTFLoader::load_nodes(const fastgltf::Asset &asset,
                            const std::vector<std::shared_ptr<Mesh>> &meshes,
                            std::shared_ptr<GLTFObj> &scene,
                            std::vector<std::shared_ptr<Node>> &nodes)
{
    nodes.reserve(asset.nodes.size());
    // Define the lambda that will load the node and the local transform
    uint32_t surfaceId = 0;
    scene->meshNodes.reserve(asset.nodes.size());
    // For now, we will consider that all nodes belong to the same scene
    for (const auto &n : asset.nodes) {
        std::shared_ptr<Node> nodeTmp;

        // If has a mesh, make the Node a MeshNode and load the mesh
        if (n.meshIndex.has_value()) {
            nodeTmp = std::make_shared<MeshNode>();
            std::shared_ptr<MeshNode> meshNodeTmp = std::dynamic_pointer_cast<MeshNode>(nodeTmp);
            const std::shared_ptr<Mesh> &mesh = meshes[n.meshIndex.value()];
            meshNodeTmp->mesh = mesh;
            meshNodeTmp->surfaceStorageBuffers.reserve(mesh->surfaces.size());
            // bufferQueue.reserve(mesh->surfaces.size());
            // Create a bound storage buffer for every surface with the device addresses
            // that will allow us access all the data from the shaders
            for (const auto &s : mesh->surfaces) {
                SurfaceStorage surfaceStorage;
                surfaceStorage.indexBufferAddress = mesh->indexBuffer->bufferAddress;
                surfaceStorage.vertexBufferAddress = mesh->vertexBuffer->bufferAddress;
                surfaceStorage.materialConstantsBufferAddress = s.material->materialResources
                                                                    .dataBuffer.bufferAddress;
                surfaceStorage.startIndex = s.startIndex;
                surfaceStorage.count = s.count;
                std::shared_ptr<Buffer> surfaceStorageBuffer = std::make_shared<Buffer>();
                *surfaceStorageBuffer
                    = utils::create_buffer(device,
                                           allocator,
                                           sizeof(SurfaceStorage),
                                           vk::BufferUsageFlagBits::eStorageBuffer,
                                           VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                                           VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
                utils::copy_to_buffer(*surfaceStorageBuffer, allocator, &surfaceStorage);
                // Store a unique surface Id for each surface. This will be used as the customInstanceID
                // when building the BLASes
                meshNodeTmp->surfaceStorageBuffers[surfaceId] = *surfaceStorageBuffer;
                surfaceId++;
                bufferQueue.emplace_back(surfaceStorageBuffer);
                scene->meshNodes.emplace_back(std::move(meshNodeTmp));
            }
        } else
            nodeTmp = std::make_shared<Node>();

        // Load the local transform
        const auto m = fastgltf::getTransformMatrix(n);
        nodeTmp->localTransform = glm::make_mat4(m.data());

        // Add the node to our structures
        nodes.emplace_back(std::move(nodeTmp));
        scene->nodes[n.name.c_str()] = nodes.back();
    }
    scene->surfaceStorageBuffersCount = surfaceId;

    // Generate the node tree structure
    for (size_t i = 0; i < nodes.size(); i++) {
        const fastgltf::Node &fgltfNode = asset.nodes[i];
        std::shared_ptr<Node> &node = nodes[i];
        node->children.reserve(fgltfNode.children.size());
        for (const size_t c : fgltfNode.children) {
            nodes[c]->parent = node;
            node->children.emplace_back(nodes[c]);
        }
    }

    // Find the top nodes and propagate/fill the worldTransform matrices to all nodes
    scene->topNodes.reserve(nodes.size());
    for (const std::shared_ptr<Node> &n : nodes) {
        if (n->parent.lock() == nullptr) {
            scene->topNodes.emplace_back(n);
            n->refreshTransform(glm::mat4(1.f));
        }
    }
}

void GLTFLoader::create_mesh_buffers(const std::vector<uint32_t> &indices,
                                     const std::vector<Vertex> &vertices,
                                     std::shared_ptr<Mesh> &mesh)
{
    mesh->indexBuffer = std::make_shared<Buffer>();
    mesh->vertexBuffer = std::make_shared<Buffer>();
    bufferQueue.reserve(bufferQueue.size() + 2);
    bufferQueue.emplace_back(mesh->indexBuffer);
    bufferQueue.emplace_back(mesh->vertexBuffer);

    const vk::DeviceSize verticesSize = vertices.size() * sizeof(Vertex);
    const vk::DeviceSize indicesSize = indices.size() * sizeof(uint32_t);

    *mesh->vertexBuffer = utils::create_buffer(
        device,
        allocator,
        verticesSize,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress
            | vk::BufferUsageFlagBits::eTransferDst
            | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

    *mesh->indexBuffer = utils::create_buffer(
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

    utils::copy_to_buffer(stagingBuffer, allocator, vertices.data(), verticesSize);
    utils::copy_to_buffer(stagingBuffer, allocator, indices.data(), indicesSize, verticesSize);

    utils::cmd_submit(device, queue, gltfFence, gltfCmd, [&](const vk::CommandBuffer &cmd) {
        // Set info structures to copy from staging to vertex & index buffers
        vk::BufferCopy2 vertexCopy{};
        vertexCopy.setSrcOffset(0);
        vertexCopy.setDstOffset(0);
        vertexCopy.setSize(verticesSize);
        vk::CopyBufferInfo2 vertexCopyInfo{};
        vertexCopyInfo.setSrcBuffer(stagingBuffer.buffer);
        vertexCopyInfo.setDstBuffer(mesh->vertexBuffer->buffer);
        vertexCopyInfo.setRegions(vertexCopy);

        vk::BufferCopy2 indexCopy{};
        indexCopy.setSrcOffset(verticesSize);
        indexCopy.setDstOffset(0);
        indexCopy.setSize(indicesSize);
        vk::CopyBufferInfo2 indexCopyInfo{};
        indexCopyInfo.setSrcBuffer(stagingBuffer.buffer);
        indexCopyInfo.setDstBuffer(mesh->indexBuffer->buffer);
        indexCopyInfo.setRegions(indexCopy);

        cmd.copyBuffer2(vertexCopyInfo);
        cmd.copyBuffer2(indexCopyInfo);
    });

    utils::destroy_buffer(allocator, stagingBuffer);
}

void GLTFLoader::destroy_buffers()
{
    for (auto &b : bufferQueue)
        utils::destroy_buffer(allocator, *b);
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
