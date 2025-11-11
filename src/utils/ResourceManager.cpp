#include "gm/utils/ResourceManager.hpp"
#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Texture.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/utils/ObjLoader.hpp"
#include "gm/core/Logger.hpp"

#include <exception>
#include <string_view>

namespace gm {

std::unordered_map<std::string, std::shared_ptr<Shader>> ResourceManager::shaders;
std::unordered_map<std::string, std::shared_ptr<Texture>> ResourceManager::textures;
std::unordered_map<std::string, std::shared_ptr<Mesh>> ResourceManager::meshes;

void ResourceManager::Init() {
    // Initialize resource maps
    shaders.clear();
    textures.clear();
    meshes.clear();
}

void ResourceManager::Cleanup() {
    shaders.clear();
    textures.clear();
    meshes.clear();
}

std::shared_ptr<Shader> ResourceManager::LoadShader(const std::string& name,
    const std::string& vertPath, const std::string& fragPath) {
    if (shaders.find(name) != shaders.end()) {
        return shaders[name];
    }
    
    auto shader = std::make_shared<Shader>();
    if (shader->loadFromFiles(vertPath, fragPath)) {
        shaders[name] = shader;
        core::Logger::Info("[ResourceManager] Loaded shader '%s' (%s, %s)",
                           name.c_str(), vertPath.c_str(), fragPath.c_str());
        return shader;
    }
    core::Logger::Error("[ResourceManager] Failed to load shader '%s' (%s, %s)",
                        name.c_str(), vertPath.c_str(), fragPath.c_str());
    return nullptr;
}

std::shared_ptr<Shader> ResourceManager::GetShader(const std::string& name) {
    auto it = shaders.find(name);
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
    // Create temporary string only for lookup (std::unordered_map requires string key)
    // This is still more efficient than passing std::string by value
    auto it = shaders.find(std::string(name));
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
    return shaders.find(std::string(name)) != shaders.end();
}

std::shared_ptr<Shader> ResourceManager::ReloadShader(const std::string& name,
    const std::string& vertPath, const std::string& fragPath) {
    auto shader = std::make_shared<Shader>();
    if (shader->loadFromFiles(vertPath, fragPath)) {
        shaders[name] = shader;
        core::Logger::Info("[ResourceManager] Reloaded shader '%s' (%s, %s)",
                           name.c_str(), vertPath.c_str(), fragPath.c_str());
        return shader;
    }
    core::Logger::Error("[ResourceManager] Failed to reload shader '%s' (%s, %s)",
                        name.c_str(), vertPath.c_str(), fragPath.c_str());
    return nullptr;
}

std::shared_ptr<Texture> ResourceManager::LoadTexture(const std::string& name,
    const std::string& path) {
    // Use find() instead of operator[] to avoid default construction
    auto it = textures.find(name);
    if (it != textures.end()) {
        return it->second;
    }
    
    try {
        auto texture = std::make_shared<Texture>(Texture::loadOrDie(path));
        textures[name] = texture;
        core::Logger::Info("[ResourceManager] Loaded texture '%s' (%s)",
                           name.c_str(), path.c_str());
        return texture;
    } catch (const std::exception& ex) {
        core::Logger::Error("[ResourceManager] Failed to load texture '%s' (%s): %s",
                            name.c_str(), path.c_str(), ex.what());
        return nullptr;
    } catch (...) {
        core::Logger::Error("[ResourceManager] Failed to load texture '%s' (%s): unknown error",
                            name.c_str(), path.c_str());
        return nullptr;
    }
}

std::shared_ptr<Texture> ResourceManager::GetTexture(const std::string& name) {
    auto it = textures.find(name);
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
    // Create temporary string only for lookup (std::unordered_map requires string key)
    // This is still more efficient than passing std::string by value
    auto it = textures.find(std::string(name));
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
    return textures.find(std::string(name)) != textures.end();
}

std::shared_ptr<Texture> ResourceManager::ReloadTexture(const std::string& name,
    const std::string& path) {
    try {
        auto texture = std::make_shared<Texture>(Texture::loadOrDie(path));
        textures[name] = texture;
        core::Logger::Info("[ResourceManager] Reloaded texture '%s' (%s)",
                           name.c_str(), path.c_str());
        return texture;
    } catch (const std::exception& ex) {
        core::Logger::Error("[ResourceManager] Failed to reload texture '%s' (%s): %s",
                            name.c_str(), path.c_str(), ex.what());
        return nullptr;
    } catch (...) {
        core::Logger::Error("[ResourceManager] Failed to reload texture '%s' (%s): unknown error",
                            name.c_str(), path.c_str());
        return nullptr;
    }
}

std::shared_ptr<Mesh> ResourceManager::LoadMesh(const std::string& name,
    const std::string& path) {
    // Use find() instead of operator[] to avoid default construction
    auto it = meshes.find(name);
    if (it != meshes.end()) {
        return it->second;
    }
    
    try {
        auto mesh = std::make_shared<Mesh>(ObjLoader::LoadObjPNUV(path));
        meshes[name] = mesh;
        core::Logger::Info("[ResourceManager] Loaded mesh '%s' (%s)",
                           name.c_str(), path.c_str());
        return mesh;
    } catch (const std::exception& ex) {
        core::Logger::Error("[ResourceManager] Failed to load mesh '%s' (%s): %s",
                            name.c_str(), path.c_str(), ex.what());
        return nullptr;
    } catch (...) {
        core::Logger::Error("[ResourceManager] Failed to load mesh '%s' (%s): unknown error",
                            name.c_str(), path.c_str());
        return nullptr;
    }
}

std::shared_ptr<Mesh> ResourceManager::GetMesh(const std::string& name) {
    auto it = meshes.find(name);
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
    // Create temporary string only for lookup (std::unordered_map requires string key)
    // This is still more efficient than passing std::string by value
    auto it = meshes.find(std::string(name));
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
    return meshes.find(std::string(name)) != meshes.end();
}

std::shared_ptr<Mesh> ResourceManager::ReloadMesh(const std::string& name,
    const std::string& path) {
    try {
        auto mesh = std::make_shared<Mesh>(ObjLoader::LoadObjPNUV(path));
        meshes[name] = mesh;
        core::Logger::Info("[ResourceManager] Reloaded mesh '%s' (%s)",
                           name.c_str(), path.c_str());
        return mesh;
    } catch (const std::exception& ex) {
        core::Logger::Error("[ResourceManager] Failed to reload mesh '%s' (%s): %s",
                            name.c_str(), path.c_str(), ex.what());
        return nullptr;
    } catch (...) {
        core::Logger::Error("[ResourceManager] Failed to reload mesh '%s' (%s): unknown error",
                            name.c_str(), path.c_str());
        return nullptr;
    }
}

}