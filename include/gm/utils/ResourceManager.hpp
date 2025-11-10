#pragma once
#include <unordered_map>
#include <string>
#include <memory>
#include "Shader.hpp"
#include "Texture.hpp"
#include "Mesh.hpp"

namespace gm {

class ResourceManager {
private:
    static std::unordered_map<std::string, std::shared_ptr<Shader>> shaders;
    static std::unordered_map<std::string, std::shared_ptr<Texture>> textures;
    static std::unordered_map<std::string, std::shared_ptr<Mesh>> meshes;

    ResourceManager() = delete;  // Static class
    
public:
    static void Init();
    static void Cleanup();
    
    static std::shared_ptr<Shader> LoadShader(const std::string& name, 
        const std::string& vertPath, const std::string& fragPath);
    static std::shared_ptr<Shader> GetShader(const std::string& name);
    
    static std::shared_ptr<Texture> LoadTexture(const std::string& name, 
        const std::string& path);
    static std::shared_ptr<Texture> GetTexture(const std::string& name);
    
    static std::shared_ptr<Mesh> LoadMesh(const std::string& name, 
        const std::string& path);
    static std::shared_ptr<Mesh> GetMesh(const std::string& name);
};

}