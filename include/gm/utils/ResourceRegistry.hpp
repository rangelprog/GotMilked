#pragma once

#include <string>
#include <unordered_map>
#include <optional>

#include <glm/vec3.hpp>

namespace gm {

class ResourceRegistry {
public:
    struct ShaderPaths {
        std::string vertPath;
        std::string fragPath;
    };

    struct MaterialData {
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

    static ResourceRegistry& Instance();

    void RegisterShader(const std::string& guid,
                        const std::string& vertPath,
                        const std::string& fragPath);
    void RegisterTexture(const std::string& guid,
                         const std::string& path);
    void RegisterMesh(const std::string& guid,
                      const std::string& path);
    void RegisterMaterial(const std::string& guid,
                          const MaterialData& material);

    std::optional<ShaderPaths> GetShaderPaths(const std::string& guid) const;
    std::optional<std::string> GetTexturePath(const std::string& guid) const;
    std::optional<std::string> GetMeshPath(const std::string& guid) const;
    std::optional<MaterialData> GetMaterialData(const std::string& guid) const;

    void UnregisterShader(const std::string& guid);
    void UnregisterTexture(const std::string& guid);
    void UnregisterMesh(const std::string& guid);
    void UnregisterMaterial(const std::string& guid);

    void Clear();

private:
    ResourceRegistry() = default;

    std::unordered_map<std::string, ShaderPaths> m_shaders;
    std::unordered_map<std::string, std::string> m_textures;
    std::unordered_map<std::string, std::string> m_meshes;
    std::unordered_map<std::string, MaterialData> m_materials;
};

} // namespace gm

