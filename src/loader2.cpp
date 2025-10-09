#include "loader2.hpp"
#include <print>

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
    std::vector<std::shared_ptr<Material2>> materials;

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
