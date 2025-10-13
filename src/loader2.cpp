#include "loader2.hpp"
#include "utils.hpp"
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

    std::shared_ptr<GLTFObj> scene = std::make_shared<GLTFObj>();

    constexpr auto options = fastgltf::Options::DontRequireValidAssetMember
                             | fastgltf::Options::LoadExternalBuffers
                             | fastgltf::Options::LoadExternalImages;

    // Load asset and check that there were no issues
    auto asset = parser.loadGltf(data.get(), path, options);
    auto val = fastgltf::validate(asset.get());
    if (asset.error() != fastgltf::Error::None || val != fastgltf::Error::None) {
        std::println("Error in parsing GLTF asset: {}", fastgltf::getErrorMessage(asset.error()));
        std::println("Error in parsing GLTF asset: {}", fastgltf::getErrorMessage(val));
        return {};
    }

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
    std::vector<std::shared_ptr<DeviceMeshAsset2>> meshes;
    std::vector<std::shared_ptr<Node>> nodes;
    std::vector<ImageData> images;
    std::vector<std::shared_ptr<GLTFMaterial>> materials;

    // Load textures
    images.reserve(asset->images.size());
    for (const fastgltf::Image &im : asset->images) {
        images.emplace_back(checkerboardImage);
    }

    // create buffer to hold the material data
    const vk::DeviceSize materialConstantsSize = sizeof(GLTFMaterial::MaterialConstants)
                                                 * asset->materials.size();
    scene->materialConstantsBuffer
        = utils::create_buffer(device,
                               allocator,
                               materialConstantsSize,
                               vk::BufferUsageFlagBits::eUniformBuffer,
                               VMA_MEMORY_USAGE_AUTO,
                               VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                   | VMA_ALLOCATION_CREATE_MAPPED_BIT);
    GLTFMaterial::MaterialConstants *pMatConstants = (GLTFMaterial::MaterialConstants *)
                                                         scene->materialConstantsBuffer
                                                             .allocationInfo.pMappedData;

    // Loop over the PBR materials
    size_t i = 0;
    materials.reserve(asset->materials.size());
    for (const fastgltf::Material &m : asset->materials) {
        i++;
        std::shared_ptr<GLTFMaterial> matTmp = std::make_shared<GLTFMaterial>();

        // Write material directly to buffer
        pMatConstants[i].baseColorFactor.x = m.pbrData.baseColorFactor.x();
        pMatConstants[i].baseColorFactor.y = m.pbrData.baseColorFactor.y();
        pMatConstants[i].baseColorFactor.z = m.pbrData.baseColorFactor.z();
        pMatConstants[i].baseColorFactor.w = m.pbrData.baseColorFactor.w();
        pMatConstants[i].metallicFactor = m.pbrData.metallicFactor;
        pMatConstants[i].roughnessFactor = m.pbrData.roughnessFactor;

        // Material type
        GLTFMaterial::MaterialPass matPass = (m.alphaMode == fastgltf::AlphaMode::Blend)
                                                 ? GLTFMaterial::MaterialPass::Transparent
                                                 : GLTFMaterial::MaterialPass::MainColor;
        // Fill materials[i]:
        materials[i]->materialPass = matPass;
        materials[i]->materialResources.colorImage = whiteImage;
        materials[i]->materialResources.colorSampler = samplerLinear;
        materials[i]->materialResources.metalRoughImage = whiteImage;
        materials[i]->materialResources.metalRoughSampler = samplerLinear;
        materials[i]->materialResources.dataBuffer = scene->materialConstantsBuffer;
        // I don't know whether should I duplicate this data:
        materials[i]->materialConstants = pMatConstants[i];
        if (m.pbrData.baseColorTexture.has_value()) {
            size_t imgIndex = asset->textures[m.pbrData.baseColorTexture->textureIndex]
                                  .imageIndex.value();
            size_t samplerIndex = asset->textures[m.pbrData.baseColorTexture->textureIndex]
                                      .samplerIndex.value();
            materials[i]->materialResources.colorImage = images[imgIndex];
            materials[i]->materialResources.colorSampler = scene->samplers[samplerIndex];
        }
    }

    return {};
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
