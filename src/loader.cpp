#include "loader.hpp"
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

std::vector<std::shared_ptr<MeshAsset>> GLTFLoader::loadGLTFMeshes(std::filesystem::__cxx11::path fp)
{
    std::cout << "Loading GLTF " << fp << std::endl;

    auto data = fastgltf::MappedGltfFile::FromPath(fp);
    if (!data) {
        std::string errStr = "Failed to open glTF file: "
                             + std::string(fastgltf::getErrorMessage(data.error())) + "\n";
        throw std::runtime_error(errStr);
    }

    fastgltf::Parser parser{};

    auto gltfTemp = parser.loadGltfBinary(data.get(),
                                          fp.parent_path(),
                                          fastgltf::Options::LoadExternalBuffers);

    if (!gltfTemp) {
        std::string errStr = "Failed to open glTF binary: "
                             + std::string(fastgltf::getErrorMessage(gltfTemp.error())) + "\n";
        throw std::runtime_error(errStr);
    }
    gltf = std::move(gltfTemp.get());

    std::vector<std::shared_ptr<MeshAsset>> meshes;
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    for (fastgltf::Mesh &mesh : gltf.meshes) {
        MeshAsset meshAsset;
        meshAsset.name = mesh.name;
        std::vector<uint32_t> indices;
        std::vector<Vertex> vertices;
        std::vector<GeoSurface> geoSurfaces;
        loadMesh(mesh, indices, vertices, geoSurfaces);
    }
    // gltf.accessors
}

void GLTFLoader::loadMesh(const fastgltf::Mesh &mesh,
                          std::vector<uint32_t> &indices,
                          std::vector<Vertex> &vertices,
                          std::vector<GeoSurface> &geoSurfaces)
{
    indices.clear();
    vertices.clear();
    geoSurfaces.reserve(mesh.primitives.size());
    for (auto &p : mesh.primitives) {
        fastgltf::Accessor &indexAccessor = gltf.accessors[p.indicesAccessor.value()];

        GeoSurface surface;
        surface.startIndex = indices.size();
        surface.count = indexAccessor.count;

        size_t verticesIndex0 = vertices.size();
        indices.reserve(indices.size() + indexAccessor.count);

        // load indices
        fastgltf::iterateAccessor<uint32_t>(gltf, indexAccessor, [&](uint32_t idx) {
            indices.push_back(idx + verticesIndex0);
        });

        // load vertices
        fastgltf::Accessor &posAccessor = gltf.accessors[p.findAttribute("POSITION")->accessorIndex];
        vertices.reserve(vertices.size() + posAccessor.count);

        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
            gltf, posAccessor, [&](fastgltf::math::fvec3 v, size_t index) {
                Vertex vertex;
                vertex.position = glm::vec3(v.x(), v.y(), v.z());
                vertex.normal = {1, 0, 0};
                vertex.color = glm::vec4{1.f};
                vertex.uvX = 0;
                vertex.uvY = 0;
                vertices.push_back(vertex);
            });

        // Find and load normals
        auto normals = p.findAttribute("NORMAL");
        fastgltf::Accessor &normAccessor = gltf.accessors[normals->accessorIndex];
        if (normals != p.attributes.end()) {
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                gltf, normAccessor, [&](fastgltf::math::fvec3 v, size_t index) {
                    vertices[verticesIndex0 + index].normal = glm::vec3(v.x(), v.y(), v.z());
                });
        }

        // load UVs
        auto uvs = p.findAttribute("TEXCOORD_0");
        fastgltf::Accessor &uvAccessor = gltf.accessors[uvs->accessorIndex];
        if (uvs != p.attributes.end()) {
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
                gltf, uvAccessor, [&](fastgltf::math::fvec2 v, size_t index) {
                    vertices[verticesIndex0 + index].uvX = v.x();
                    vertices[verticesIndex0 + index].uvY = v.y();
                });
        }

        // load vertex colors
        auto colors = p.findAttribute("COLOR_0");
        fastgltf::Accessor &colorAccessor = gltf.accessors[colors->accessorIndex];
        if (colors != p.attributes.end()) {
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(
                gltf, colorAccessor, [&](fastgltf::math::fvec4 v, size_t index) {
                    vertices[verticesIndex0 + index].color = glm::vec4(v.x(), v.y(), v.z(), v.w());
                });
        }

        geoSurfaces.push_back(surface);
    }
}
