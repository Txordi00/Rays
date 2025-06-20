#pragma once
#include "types.hpp"
#include <fastgltf/types.hpp>
#include <filesystem>
#include <memory>

struct GeoSurface
{
    uint32_t startIndex;
    uint32_t count;
};

struct MeshAsset
{
    std::string name;
    std::vector<GeoSurface> geoSurfaces;
    // MeshBuffer meshBuffer;
};

class GLTFLoader
{
public:
    GLTFLoader() = default;
    ~GLTFLoader() = default;
    std::vector<std::shared_ptr<MeshAsset>> loadGLTFMeshes(std::filesystem::path fp);
    void loadMesh(const fastgltf::Mesh &mesh,
                  std::vector<uint32_t> &indices,
                  std::vector<Vertex> &vertices,
                  std::vector<GeoSurface> &geoSurfaces);

private:
    fastgltf::Asset gltf;
};
