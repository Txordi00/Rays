#pragma once
#include "fastgltf/core.hpp"
#include "types.hpp"
#include <fastgltf/types.hpp>
#include <filesystem>
#include <memory>

struct GeoSurface
{
    uint32_t startIndex;
    uint32_t count;
};

struct HostMeshAsset
{
    std::string name;
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;
    std::vector<GeoSurface> surfaces;
};

struct DeviceMeshAsset
{
    std::string name;
    std::vector<GeoSurface> surfaces;
    MeshBuffer meshBuffer;
};

class GLTFLoader
{
public:
    GLTFLoader() = default;
    ~GLTFLoader() = default;
    std::vector<std::shared_ptr<HostMeshAsset>> loadGLTFMeshes(const std::filesystem::path &fp);
    void loadMesh(const fastgltf::Mesh &mesh,
                  std::vector<uint32_t> &indices,
                  std::vector<Vertex> &vertices,
                  std::vector<GeoSurface> &surfaces);

    bool overrideColorsWithNormals = true;

private:
    fastgltf::Parser parser{};
    fastgltf::Asset gltf;
};
