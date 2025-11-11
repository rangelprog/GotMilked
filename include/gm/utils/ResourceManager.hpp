#pragma once
#include <unordered_map>
#include <string>
#include <string_view>
#include <memory>

namespace gm {

class Shader;
class Texture;
class Mesh;

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
    // Optimized lookup overloads - avoid string copies in hot paths
    static std::shared_ptr<Shader> GetShader(const std::string& name);
    static std::shared_ptr<Shader> GetShader(const char* name);  // Avoid string copy
    static std::shared_ptr<Shader> GetShader(std::string_view name);  // Avoid string copy
    static bool HasShader(const std::string& name);
    static bool HasShader(const char* name);  // Avoid string copy
    static bool HasShader(std::string_view name);  // Avoid string copy
    static std::shared_ptr<Shader> ReloadShader(const std::string& name,
        const std::string& vertPath, const std::string& fragPath);
    
    static std::shared_ptr<Texture> LoadTexture(const std::string& name, 
        const std::string& path);
    // Optimized lookup overloads - avoid string copies in hot paths
    static std::shared_ptr<Texture> GetTexture(const std::string& name);
    static std::shared_ptr<Texture> GetTexture(const char* name);  // Avoid string copy
    static std::shared_ptr<Texture> GetTexture(std::string_view name);  // Avoid string copy
    static bool HasTexture(const std::string& name);
    static bool HasTexture(const char* name);  // Avoid string copy
    static bool HasTexture(std::string_view name);  // Avoid string copy
    static std::shared_ptr<Texture> ReloadTexture(const std::string& name,
        const std::string& path);
    
    static std::shared_ptr<Mesh> LoadMesh(const std::string& name, 
        const std::string& path);
    // Optimized lookup overloads - avoid string copies in hot paths
    static std::shared_ptr<Mesh> GetMesh(const std::string& name);
    static std::shared_ptr<Mesh> GetMesh(const char* name);  // Avoid string copy
    static std::shared_ptr<Mesh> GetMesh(std::string_view name);  // Avoid string copy
    static bool HasMesh(const std::string& name);
    static bool HasMesh(const char* name);  // Avoid string copy
    static bool HasMesh(std::string_view name);  // Avoid string copy
    static std::shared_ptr<Mesh> ReloadMesh(const std::string& name,
        const std::string& path);
};

}