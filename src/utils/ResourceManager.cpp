#include "gm/utils/ResourceManager.hpp"
#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Texture.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/rendering/RenderStateCache.hpp"
#include "gm/utils/ObjLoader.hpp"
#include "gm/core/Error.hpp"
#include "gm/core/Logger.hpp"

#include <exception>
#include <sstream>

namespace gm {

std::unordered_map<std::string, std::shared_ptr<Shader>> ResourceManager::shaders;
std::unordered_map<std::string, std::shared_ptr<Texture>> ResourceManager::textures;
std::unordered_map<std::string, std::shared_ptr<Mesh>> ResourceManager::meshes;

void ResourceManager::Init() {
    shaders.clear();
    textures.clear();
    meshes.clear();
    RenderStateCache::Reset();
}

void ResourceManager::Cleanup() {
    shaders.clear();
    textures.clear();
    meshes.clear();
    RenderStateCache::Reset();
}

ResourceManager::ShaderHandle ResourceManager::LoadShader(const ShaderDescriptor& descriptor) {
    if (descriptor.guid.empty()) {
        throw core::ResourceError("shader", descriptor.guid, "Shader descriptor GUID is empty");
    }

    if (auto it = shaders.find(descriptor.guid); it != shaders.end()) {
        return MakeHandle(descriptor.guid, it->second);
    }

    auto shader = std::make_shared<Shader>();
    if (!shader->loadFromFiles(descriptor.vertexPath, descriptor.fragmentPath)) {
        std::ostringstream oss;
        oss << "Failed to load shader (" << descriptor.vertexPath << ", " << descriptor.fragmentPath << ")";
        throw core::ResourceError("shader", descriptor.guid, oss.str());
    }

    shaders[descriptor.guid] = shader;
    core::Logger::Info("[ResourceManager] Loaded shader '{}' ({}, {})",
                       descriptor.guid, descriptor.vertexPath, descriptor.fragmentPath);
    return MakeHandle(descriptor.guid, shader);
}

ResourceManager::ShaderHandle ResourceManager::ReloadShader(const ShaderDescriptor& descriptor) {
    if (descriptor.guid.empty()) {
        throw core::ResourceError("shader", descriptor.guid, "Shader descriptor GUID is empty");
    }

    auto shader = std::make_shared<Shader>();
    if (!shader->loadFromFiles(descriptor.vertexPath, descriptor.fragmentPath)) {
        std::ostringstream oss;
        oss << "Failed to reload shader (" << descriptor.vertexPath << ", " << descriptor.fragmentPath << ")";
        throw core::ResourceError("shader", descriptor.guid, oss.str());
    }

    if (auto it = shaders.find(descriptor.guid); it != shaders.end()) {
        if (it->second) {
            RenderStateCache::InvalidateShader(it->second->Id());
        }
    }

    shaders[descriptor.guid] = shader;
    RenderStateCache::InvalidateShader(shader->Id());
    core::Logger::Info("[ResourceManager] Reloaded shader '{}' ({}, {})",
                       descriptor.guid, descriptor.vertexPath, descriptor.fragmentPath);
    return MakeHandle(descriptor.guid, shader);
}

std::shared_ptr<Shader> ResourceManager::GetShader(const std::string& guid) {
    if (auto it = shaders.find(guid); it != shaders.end()) {
        return it->second;
    }
    return nullptr;
}

bool ResourceManager::HasShader(const std::string& guid) {
    return shaders.find(guid) != shaders.end();
}

ResourceManager::TextureHandle ResourceManager::LoadTexture(const TextureDescriptor& descriptor) {
    if (descriptor.guid.empty()) {
        throw core::ResourceError("texture", descriptor.guid, "Texture descriptor GUID is empty");
    }

    if (auto it = textures.find(descriptor.guid); it != textures.end()) {
        return MakeHandle(descriptor.guid, it->second);
    }

    try {
        auto texture = std::make_shared<Texture>(Texture::loadOrThrow(descriptor.path));
        textures[descriptor.guid] = texture;
        core::Logger::Info("[ResourceManager] Loaded texture '{}' ({})",
                           descriptor.guid, descriptor.path);
        return MakeHandle(descriptor.guid, texture);
    } catch (const core::GraphicsError& err) {
        throw core::ResourceError("texture", descriptor.guid, std::string(err.what()));
    } catch (const std::exception& ex) {
        throw core::ResourceError("texture", descriptor.guid, ex.what());
    }
}

ResourceManager::TextureHandle ResourceManager::ReloadTexture(const TextureDescriptor& descriptor) {
    if (descriptor.guid.empty()) {
        throw core::ResourceError("texture", descriptor.guid, "Texture descriptor GUID is empty");
    }

    try {
        auto texture = std::make_shared<Texture>(Texture::loadOrThrow(descriptor.path));
        if (auto it = textures.find(descriptor.guid); it != textures.end()) {
            if (it->second) {
                RenderStateCache::InvalidateTexture(it->second->id());
            }
        }
        textures[descriptor.guid] = texture;
        RenderStateCache::InvalidateTexture(texture->id());
        core::Logger::Info("[ResourceManager] Reloaded texture '{}' ({})",
                           descriptor.guid, descriptor.path);
        return MakeHandle(descriptor.guid, texture);
    } catch (const core::GraphicsError& err) {
        throw core::ResourceError("texture", descriptor.guid, std::string(err.what()));
    } catch (const std::exception& ex) {
        throw core::ResourceError("texture", descriptor.guid, ex.what());
    }
}

std::shared_ptr<Texture> ResourceManager::GetTexture(const std::string& guid) {
    if (auto it = textures.find(guid); it != textures.end()) {
        return it->second;
    }
    return nullptr;
}

bool ResourceManager::HasTexture(const std::string& guid) {
    return textures.find(guid) != textures.end();
}

ResourceManager::MeshHandle ResourceManager::LoadMesh(const MeshDescriptor& descriptor) {
    if (descriptor.guid.empty()) {
        throw core::ResourceError("mesh", descriptor.guid, "Mesh descriptor GUID is empty");
    }

    if (auto it = meshes.find(descriptor.guid); it != meshes.end()) {
        return MakeHandle(descriptor.guid, it->second);
    }

    try {
        auto mesh = std::make_shared<Mesh>(ObjLoader::LoadObjPNUV(descriptor.path));
        meshes[descriptor.guid] = mesh;
        core::Logger::Info("[ResourceManager] Loaded mesh '{}' ({})",
                           descriptor.guid, descriptor.path);
        return MakeHandle(descriptor.guid, mesh);
    } catch (const core::ResourceError&) {
        throw;
    } catch (const std::exception& ex) {
        throw core::ResourceError("mesh", descriptor.guid, ex.what());
    }
}

ResourceManager::MeshHandle ResourceManager::ReloadMesh(const MeshDescriptor& descriptor) {
    if (descriptor.guid.empty()) {
        throw core::ResourceError("mesh", descriptor.guid, "Mesh descriptor GUID is empty");
    }

    try {
        auto mesh = std::make_shared<Mesh>(ObjLoader::LoadObjPNUV(descriptor.path));
        meshes[descriptor.guid] = mesh;
        core::Logger::Info("[ResourceManager] Reloaded mesh '{}' ({})",
                           descriptor.guid, descriptor.path);
        return MakeHandle(descriptor.guid, mesh);
    } catch (const core::ResourceError&) {
        throw;
    } catch (const std::exception& ex) {
        throw core::ResourceError("mesh", descriptor.guid, ex.what());
    }
}

std::shared_ptr<Mesh> ResourceManager::GetMesh(const std::string& guid) {
    if (auto it = meshes.find(guid); it != meshes.end()) {
        return it->second;
    }
    return nullptr;
}

bool ResourceManager::HasMesh(const std::string& guid) {
    return meshes.find(guid) != meshes.end();
}

} // namespace gm