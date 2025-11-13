#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace gm {

class Shader;
class Texture;
class Mesh;

class ResourceManager {
public:
    template <typename T>
    class ResourceHandle {
    public:
        ResourceHandle() = default;
        ResourceHandle(std::string guid, std::weak_ptr<T> resource)
            : m_guid(std::move(guid)), m_resource(std::move(resource)) {}

        const std::string& Guid() const { return m_guid; }
        bool IsValid() const { return !m_guid.empty(); }
        bool IsLoaded() const { return !m_resource.expired(); }
        std::shared_ptr<T> Lock() const { return m_resource.lock(); }

        explicit operator bool() const { return IsValid(); }

    private:
        std::string m_guid;
        std::weak_ptr<T> m_resource;
    };

    struct ShaderDescriptor {
        std::string guid;
        std::string vertexPath;
        std::string fragmentPath;
    };

    struct TextureDescriptor {
        std::string guid;
        std::string path;
        bool generateMipmaps = true;
        bool srgb = true;
        bool flipY = true;
    };

    struct MeshDescriptor {
        std::string guid;
        std::string path;
    };

    using ShaderHandle = ResourceHandle<Shader>;
    using TextureHandle = ResourceHandle<Texture>;
    using MeshHandle = ResourceHandle<Mesh>;

    static void Init();
    static void Cleanup();

    static ShaderHandle LoadShader(const ShaderDescriptor& descriptor);
    static ShaderHandle ReloadShader(const ShaderDescriptor& descriptor);
    static std::shared_ptr<Shader> GetShader(const std::string& guid);
    static bool HasShader(const std::string& guid);

    static TextureHandle LoadTexture(const TextureDescriptor& descriptor);
    static TextureHandle ReloadTexture(const TextureDescriptor& descriptor);
    static std::shared_ptr<Texture> GetTexture(const std::string& guid);
    static bool HasTexture(const std::string& guid);

    static MeshHandle LoadMesh(const MeshDescriptor& descriptor);
    static MeshHandle ReloadMesh(const MeshDescriptor& descriptor);
    static std::shared_ptr<Mesh> GetMesh(const std::string& guid);
    static bool HasMesh(const std::string& guid);

private:
    ResourceManager() = delete;

    template <typename T>
    static ResourceHandle<T> MakeHandle(const std::string& guid, const std::shared_ptr<T>& resource) {
        return ResourceHandle<T>(guid, resource);
    }

    static std::unordered_map<std::string, std::shared_ptr<Shader>> shaders;
    static std::unordered_map<std::string, std::shared_ptr<Texture>> textures;
    static std::unordered_map<std::string, std::shared_ptr<Mesh>> meshes;
};

} // namespace gm
