#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

namespace gm::utils {

struct ResourceManifest {
    struct ShaderEntry {
        std::string guid;
        std::string vertexPath;
        std::string fragmentPath;
    };

    struct TextureEntry {
        std::string guid;
        std::string path;
        bool generateMipmaps = true;
        bool srgb = true;
        bool flipY = true;
    };

    struct MeshEntry {
        std::string guid;
        std::string path;
    };

    struct MaterialEntry {
        std::string guid;
        std::string name;
        glm::vec3 diffuseColor{1.0f};
        glm::vec3 specularColor{1.0f};
        glm::vec3 emissionColor{0.0f};
        float shininess = 32.0f;
        std::optional<std::string> diffuseTextureGuid;
        std::optional<std::string> specularTextureGuid;
        std::optional<std::string> normalTextureGuid;
        std::optional<std::string> emissionTextureGuid;
    };

    struct Defaults {
        std::optional<std::string> shaderGuid;
        std::optional<std::string> textureGuid;
        std::optional<std::string> meshGuid;
        std::optional<std::string> terrainMaterialGuid;
    };

    std::vector<ShaderEntry> shaders;
    std::vector<TextureEntry> textures;
    std::vector<MeshEntry> meshes;
    std::vector<MaterialEntry> materials;
    Defaults defaults;
};

struct ResourceManifestLoadResult {
    bool success = false;
    ResourceManifest manifest;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

ResourceManifestLoadResult LoadResourceManifest(const std::filesystem::path& manifestPath);

} // namespace gm::utils


