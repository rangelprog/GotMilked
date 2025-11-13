#include "gm/utils/ResourceRegistry.hpp"

namespace gm {

ResourceRegistry& ResourceRegistry::Instance() {
    static ResourceRegistry instance;
    return instance;
}

void ResourceRegistry::RegisterShader(const std::string& guid,
                                      const std::string& vertPath,
                                      const std::string& fragPath) {
    if (guid.empty())
        return;
    m_shaders[guid] = ShaderPaths{vertPath, fragPath};
}

void ResourceRegistry::RegisterTexture(const std::string& guid,
                                       const std::string& path) {
    if (guid.empty())
        return;
    m_textures[guid] = path;
}

void ResourceRegistry::RegisterMesh(const std::string& guid,
                                    const std::string& path) {
    if (guid.empty())
        return;
    m_meshes[guid] = path;
}

void ResourceRegistry::RegisterMaterial(const std::string& guid,
                                        const MaterialData& material) {
    if (guid.empty())
        return;
    m_materials[guid] = material;
}

std::optional<ResourceRegistry::ShaderPaths> ResourceRegistry::GetShaderPaths(const std::string& guid) const {
    auto it = m_shaders.find(guid);
    if (it != m_shaders.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<std::string> ResourceRegistry::GetTexturePath(const std::string& guid) const {
    auto it = m_textures.find(guid);
    if (it != m_textures.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<std::string> ResourceRegistry::GetMeshPath(const std::string& guid) const {
    auto it = m_meshes.find(guid);
    if (it != m_meshes.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<ResourceRegistry::MaterialData> ResourceRegistry::GetMaterialData(const std::string& guid) const {
    auto it = m_materials.find(guid);
    if (it != m_materials.end()) {
        return it->second;
    }
    return std::nullopt;
}

void ResourceRegistry::UnregisterShader(const std::string& guid) {
    if (guid.empty())
        return;
    m_shaders.erase(guid);
}

void ResourceRegistry::UnregisterTexture(const std::string& guid) {
    if (guid.empty())
        return;
    m_textures.erase(guid);
}

void ResourceRegistry::UnregisterMesh(const std::string& guid) {
    if (guid.empty())
        return;
    m_meshes.erase(guid);
}

void ResourceRegistry::UnregisterMaterial(const std::string& guid) {
    if (guid.empty())
        return;
    m_materials.erase(guid);
}

void ResourceRegistry::Clear() {
    m_shaders.clear();
    m_textures.clear();
    m_meshes.clear();
    m_materials.clear();
}

} // namespace gm

