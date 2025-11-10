#include "gm/utils/ResourceManager.hpp"
#include "gm/utils/ObjLoader.hpp"

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
        return shader;
    }
    return nullptr;
}

std::shared_ptr<Shader> ResourceManager::GetShader(const std::string& name) {
    auto it = shaders.find(name);
    if (it != shaders.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<Texture> ResourceManager::LoadTexture(const std::string& name, 
    const std::string& path) {
    if (textures.find(name) != textures.end()) {
        return textures[name];
    }
    
    try {
        auto texture = std::make_shared<Texture>(Texture::loadOrDie(path));
        textures[name] = texture;
        return texture;
    } catch (...) {
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

std::shared_ptr<Mesh> ResourceManager::LoadMesh(const std::string& name, 
    const std::string& path) {
    if (meshes.find(name) != meshes.end()) {
        return meshes[name];
    }
    
    try {
        auto mesh = std::make_shared<Mesh>(ObjLoader::LoadObjPNUV(path));
        meshes[name] = mesh;
        return mesh;
    } catch (...) {
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

}