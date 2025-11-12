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
#include <string_view>
#include <cstring>

namespace gm {

ResourceManager::ShaderMap ResourceManager::shaders;
ResourceManager::TextureMap ResourceManager::textures;
ResourceManager::MeshMap ResourceManager::meshes;
ResourceManager::StringSet ResourceManager::internedStrings;
std::vector<std::unique_ptr<char[]>> ResourceManager::stringStorage;
std::mutex ResourceManager::stringPoolMutex;

const char* ResourceManager::InternString(std::string_view value) {
    std::lock_guard<std::mutex> lock(stringPoolMutex);

    auto it = internedStrings.find(value);
    if (it != internedStrings.end()) {
        return *it;
    }

    auto buffer = std::make_unique<char[]>(value.size() + 1);
    std::memcpy(buffer.get(), value.data(), value.size());
    buffer[value.size()] = '\0';

    const char* stored = buffer.get();
    stringStorage.emplace_back(std::move(buffer));
    internedStrings.insert(stored);
    return stored;
}

void ResourceManager::ClearInternedStrings() {
    std::lock_guard<std::mutex> lock(stringPoolMutex);
    internedStrings.clear();
    stringStorage.clear();
}

void ResourceManager::Init() {
    // Initialize resource maps
    shaders.clear();
    textures.clear();
    meshes.clear();
    ClearInternedStrings();
    RenderStateCache::Reset();
}

void ResourceManager::Cleanup() {
    shaders.clear();
    textures.clear();
    meshes.clear();
    ClearInternedStrings();
    RenderStateCache::Reset();
}

std::shared_ptr<Shader> ResourceManager::LoadShader(const std::string& name,
    const std::string& vertPath, const std::string& fragPath) {
    if (auto it = shaders.find(std::string_view{name}); it != shaders.end()) {
        return it->second;
    }
    
    auto shader = std::make_shared<Shader>();
    if (!shader->loadFromFiles(vertPath, fragPath)) {
        std::ostringstream oss;
        oss << "Failed to load shader (" << vertPath << ", " << fragPath << ")";
        throw core::ResourceError("shader", name, oss.str());
    }

    const char* pooledName = InternString(name);
    shaders[pooledName] = shader;
    core::Logger::Info("[ResourceManager] Loaded shader '{}' ({}, {})",
                       name, vertPath, fragPath);
    return shader;
}

std::shared_ptr<Shader> ResourceManager::GetShader(const std::string& name) {
    auto it = shaders.find(std::string_view{name});
    if (it != shaders.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<Shader> ResourceManager::GetShader(const char* name) {
    // Use string_view for lookup to avoid string allocation
    return GetShader(std::string_view(name));
}

std::shared_ptr<Shader> ResourceManager::GetShader(std::string_view name) {
    auto it = shaders.find(name);
    if (it != shaders.end()) {
        return it->second;
    }
    return nullptr;
}

bool ResourceManager::HasShader(const std::string& name) {
    return shaders.find(name) != shaders.end();
}

bool ResourceManager::HasShader(const char* name) {
    return HasShader(std::string_view(name));
}

bool ResourceManager::HasShader(std::string_view name) {
    return shaders.find(name) != shaders.end();
}

std::shared_ptr<Shader> ResourceManager::ReloadShader(const std::string& name,
    const std::string& vertPath, const std::string& fragPath) {
    auto shader = std::make_shared<Shader>();
    if (!shader->loadFromFiles(vertPath, fragPath)) {
        std::ostringstream oss;
        oss << "Failed to reload shader (" << vertPath << ", " << fragPath << ")";
        throw core::ResourceError("shader", name, oss.str());
    }

    const char* pooledName = InternString(name);
    if (auto existing = shaders.find(std::string_view{name}); existing != shaders.end()) {
        if (existing->second) {
            RenderStateCache::InvalidateShader(existing->second->Id());
        }
    }
    shaders[pooledName] = shader;
    RenderStateCache::InvalidateShader(shader->Id());
    core::Logger::Info("[ResourceManager] Reloaded shader '{}' ({}, {})",
                       name, vertPath, fragPath);
    return shader;
}

std::shared_ptr<Texture> ResourceManager::LoadTexture(const std::string& name,
    const std::string& path) {
    auto it = textures.find(std::string_view{name});
    if (it != textures.end()) {
        return it->second;
    }
    
    try {
        auto texture = std::make_shared<Texture>(Texture::loadOrThrow(path));
        const char* pooledName = InternString(name);
        textures[pooledName] = texture;
        core::Logger::Info("[ResourceManager] Loaded texture '{}' ({})",
                           name, path);
        return texture;
    } catch (const core::GraphicsError& err) {
        throw core::ResourceError("texture", name, std::string(err.what()));
    } catch (const std::exception& ex) {
        throw core::ResourceError("texture", name, ex.what());
    }
}

std::shared_ptr<Texture> ResourceManager::GetTexture(const std::string& name) {
    auto it = textures.find(std::string_view{name});
    if (it != textures.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<Texture> ResourceManager::GetTexture(const char* name) {
    // Use string_view for lookup to avoid string allocation
    return GetTexture(std::string_view(name));
}

std::shared_ptr<Texture> ResourceManager::GetTexture(std::string_view name) {
    auto it = textures.find(name);
    if (it != textures.end()) {
        return it->second;
    }
    return nullptr;
}

bool ResourceManager::HasTexture(const std::string& name) {
    return textures.find(name) != textures.end();
}

bool ResourceManager::HasTexture(const char* name) {
    return HasTexture(std::string_view(name));
}

bool ResourceManager::HasTexture(std::string_view name) {
    return textures.find(name) != textures.end();
}

std::shared_ptr<Texture> ResourceManager::ReloadTexture(const std::string& name,
    const std::string& path) {
    try {
        auto texture = std::make_shared<Texture>(Texture::loadOrThrow(path));
        const char* pooledName = InternString(name);
        if (auto existing = textures.find(std::string_view{name}); existing != textures.end()) {
            if (existing->second) {
                RenderStateCache::InvalidateTexture(existing->second->id());
            }
        }
        textures[pooledName] = texture;
        RenderStateCache::InvalidateTexture(texture->id());
        core::Logger::Info("[ResourceManager] Reloaded texture '{}' ({})",
                           name, path);
        return texture;
    } catch (const core::GraphicsError& err) {
        throw core::ResourceError("texture", name, std::string(err.what()));
    } catch (const std::exception& ex) {
        throw core::ResourceError("texture", name, ex.what());
    }
}

std::shared_ptr<Mesh> ResourceManager::LoadMesh(const std::string& name,
    const std::string& path) {
    auto it = meshes.find(std::string_view{name});
    if (it != meshes.end()) {
        return it->second;
    }
    
    try {
        auto mesh = std::make_shared<Mesh>(ObjLoader::LoadObjPNUV(path));
        const char* pooledName = InternString(name);
        meshes[pooledName] = mesh;
        core::Logger::Info("[ResourceManager] Loaded mesh '{}' ({})",
                           name, path);
        return mesh;
    } catch (const core::ResourceError&) {
        throw;
    } catch (const std::exception& ex) {
        throw core::ResourceError("mesh", name, ex.what());
    }
}

std::shared_ptr<Mesh> ResourceManager::GetMesh(const std::string& name) {
    auto it = meshes.find(std::string_view{name});
    if (it != meshes.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<Mesh> ResourceManager::GetMesh(const char* name) {
    // Use string_view for lookup to avoid string allocation
    return GetMesh(std::string_view(name));
}

std::shared_ptr<Mesh> ResourceManager::GetMesh(std::string_view name) {
    auto it = meshes.find(name);
    if (it != meshes.end()) {
        return it->second;
    }
    return nullptr;
}

bool ResourceManager::HasMesh(const std::string& name) {
    return meshes.find(name) != meshes.end();
}

bool ResourceManager::HasMesh(const char* name) {
    return HasMesh(std::string_view(name));
}

bool ResourceManager::HasMesh(std::string_view name) {
    return meshes.find(name) != meshes.end();
}

std::shared_ptr<Mesh> ResourceManager::ReloadMesh(const std::string& name,
    const std::string& path) {
    try {
        auto mesh = std::make_shared<Mesh>(ObjLoader::LoadObjPNUV(path));
        const char* pooledName = InternString(name);
        meshes[pooledName] = mesh;
        core::Logger::Info("[ResourceManager] Reloaded mesh '{}' ({})",
                           name, path);
        return mesh;
    } catch (const core::ResourceError&) {
        throw;
    } catch (const std::exception& ex) {
        throw core::ResourceError("mesh", name, ex.what());
    }
}

}