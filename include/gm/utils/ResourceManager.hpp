#pragma once
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <string_view>
#include <memory>
#include <mutex>
#include <vector>
#include <cstring>

namespace gm {

class Shader;
class Texture;
class Mesh;

class ResourceManager {
private:
    struct CStringHash {
        using is_transparent = void;
        std::size_t operator()(const char* value) const noexcept {
            return std::hash<std::string_view>{}(std::string_view{value});
        }
        std::size_t operator()(std::string_view value) const noexcept {
            return std::hash<std::string_view>{}(value);
        }
    };

    struct CStringEqual {
        using is_transparent = void;
        bool operator()(const char* lhs, const char* rhs) const noexcept {
            return std::strcmp(lhs, rhs) == 0;
        }
        bool operator()(std::string_view lhs, const char* rhs) const noexcept {
            return lhs == std::string_view{rhs};
        }
        bool operator()(const char* lhs, std::string_view rhs) const noexcept {
            return std::string_view{lhs} == rhs;
        }
        bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
            return lhs == rhs;
        }
    };

    using ShaderMap = std::unordered_map<const char*, std::shared_ptr<Shader>, CStringHash, CStringEqual>;
    using TextureMap = std::unordered_map<const char*, std::shared_ptr<Texture>, CStringHash, CStringEqual>;
    using MeshMap   = std::unordered_map<const char*, std::shared_ptr<Mesh>, CStringHash, CStringEqual>;
    using StringSet = std::unordered_set<const char*, CStringHash, CStringEqual>;

    static ShaderMap shaders;
    static TextureMap textures;
    static MeshMap meshes;

    static StringSet internedStrings;
    static std::vector<std::unique_ptr<char[]>> stringStorage;
    static std::mutex stringPoolMutex;

    static const char* InternString(std::string_view value);
    static void ClearInternedStrings();

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