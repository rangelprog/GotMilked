#pragma once

#include <string>
#include <unordered_map>
#include <optional>

namespace gm {

class ResourceRegistry {
public:
    struct ShaderPaths {
        std::string vertPath;
        std::string fragPath;
    };

    static ResourceRegistry& Instance();

    void RegisterShader(const std::string& guid,
                        const std::string& vertPath,
                        const std::string& fragPath);
    void RegisterTexture(const std::string& guid,
                         const std::string& path);
    void RegisterMesh(const std::string& guid,
                      const std::string& path);

    std::optional<ShaderPaths> GetShaderPaths(const std::string& guid) const;
    std::optional<std::string> GetTexturePath(const std::string& guid) const;
    std::optional<std::string> GetMeshPath(const std::string& guid) const;

    void UnregisterShader(const std::string& guid);
    void UnregisterTexture(const std::string& guid);
    void UnregisterMesh(const std::string& guid);

    void Clear();

private:
    ResourceRegistry() = default;

    std::unordered_map<std::string, ShaderPaths> m_shaders;
    std::unordered_map<std::string, std::string> m_textures;
    std::unordered_map<std::string, std::string> m_meshes;
};

} // namespace gm

