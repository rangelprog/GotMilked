#pragma once

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Texture.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/rendering/Material.hpp"
#include "gm/utils/ResourceManifest.hpp"
#include "gm/core/Error.hpp"
#include "gm/assets/AssetCatalog.hpp"

// Forward declaration for test helper (defined in tests)
struct TestAssetBundle;
void PopulateGameResourcesFromTestAssets(const TestAssetBundle& bundle, class GameResources& resources);

class GameResources {
    // Friend declaration for test helper
    friend void PopulateGameResourcesFromTestAssets(const TestAssetBundle&, GameResources&);

public:
    ~GameResources();
    /**
     * @brief Load resources from the given assets directory using the asset catalog and optional metadata manifest.
     * @param assetsDir Base assets directory
     * @return true if loading succeeded, false otherwise
     */
    bool Load(const std::filesystem::path& assetsDir);
    bool Load(const std::string& assetsDir) { return Load(std::filesystem::path(assetsDir)); }
    bool ReloadShader();
    bool ReloadShader(const std::string& guid);
    bool ReloadTexture();
    bool ReloadTexture(const std::string& guid);
    bool ReloadMesh();
    bool ReloadMesh(const std::string& guid);
    bool ReloadAll();
    void Release();

    const gm::core::Error* GetLastError() const { return m_lastError.get(); }

    struct CatalogUpdateResult {
        bool hadEvents = false;
        bool reloadSucceeded = false;
        bool prefabsChanged = false;

        explicit operator bool() const { return hadEvents; }
    };

    CatalogUpdateResult ProcessCatalogEvents();

    // Resource accessors
    gm::Shader* GetShader(const std::string& guid) const;
    gm::Texture* GetTexture(const std::string& guid) const;
    gm::Mesh* GetMesh(const std::string& guid) const;
    std::shared_ptr<gm::Material> GetMaterial(const std::string& guid) const;

    gm::Shader* GetDefaultShader() const;
    gm::Texture* GetDefaultTexture() const;
    gm::Mesh* GetDefaultMesh() const;
    std::shared_ptr<gm::Material> GetTerrainMaterial() const;

    gm::Shader* GetShader() const { return GetDefaultShader(); }
    gm::Texture* GetTexture() const { return GetDefaultTexture(); }
    gm::Mesh* GetMesh() const { return GetDefaultMesh(); }

    // Path accessors (for hot reload)
    const std::string& GetShaderVertPath() const { return m_defaultShaderVertPath; }
    const std::string& GetShaderFragPath() const { return m_defaultShaderFragPath; }
    const std::string& GetTexturePath() const { return m_defaultTexturePath; }
    const std::string& GetMeshPath() const { return m_defaultMeshPath; }
    const std::string& GetShaderGuid() const { return m_defaultShaderGuid; }
    const std::string& GetTextureGuid() const { return m_defaultTextureGuid; }
    const std::string& GetMeshGuid() const { return m_defaultMeshGuid; }

    std::optional<gm::utils::ResourceManifest::ShaderEntry> GetShaderSource(const std::string& guid) const;
    std::optional<std::string> GetTextureSource(const std::string& guid) const;
    std::optional<std::string> GetMeshSource(const std::string& guid) const;

    const std::unordered_map<std::string, std::shared_ptr<gm::Material>>& GetMaterialMap() const { return m_materials; }
    const std::unordered_map<std::string, std::shared_ptr<gm::Shader>>& GetShaderMap() const { return m_shaders; }
    const std::unordered_map<std::string, std::shared_ptr<gm::Texture>>& GetTextureMap() const { return m_textures; }
    std::shared_ptr<gm::Texture> GetTextureShared(const std::string& guid) const;
    const std::unordered_map<std::string, std::shared_ptr<gm::Mesh>>& GetMeshMap() const { return m_meshes; }
    const std::unordered_map<std::string, std::string>& GetPrefabMap() const { return m_prefabSources; }

    const std::filesystem::path& GetAssetsDirectory() const { return m_assetsDir; }

    void EnsureTextureRegistered(const std::string& guid, std::shared_ptr<gm::Texture> texture);
    std::shared_ptr<gm::Texture> EnsureTextureAvailable(const std::string& guid);

private:
    bool LoadInternal(const std::filesystem::path& assetsDir);
    void StoreError(const gm::core::Error& err);
    void RegisterDefaults();

    struct ShaderSources {
        std::string vertPath;
        std::string fragPath;
    };

    std::unordered_map<std::string, std::shared_ptr<gm::Shader>> m_shaders;
    std::unordered_map<std::string, std::shared_ptr<gm::Texture>> m_textures;
    std::unordered_map<std::string, std::shared_ptr<gm::Mesh>> m_meshes;
    std::unordered_map<std::string, std::shared_ptr<gm::Material>> m_materials;

    std::unordered_map<std::string, ShaderSources> m_shaderSources;
    std::unordered_map<std::string, gm::utils::ResourceManifest::TextureEntry> m_textureSources;
    std::unordered_map<std::string, gm::utils::ResourceManifest::MeshEntry> m_meshSources;
    std::unordered_map<std::string, gm::utils::ResourceManifest::MaterialEntry> m_materialSources;
    std::unordered_map<std::string, std::string> m_prefabSources;

    std::string m_defaultShaderGuid;
    std::string m_defaultShaderVertPath;
    std::string m_defaultShaderFragPath;
    std::string m_defaultTextureGuid;
    std::string m_defaultTexturePath;
    std::string m_defaultMeshGuid;
    std::string m_defaultMeshPath;
    std::string m_defaultTerrainMaterialGuid;

    std::filesystem::path m_assetsDir;
    std::shared_ptr<gm::core::Error> m_lastError;

    gm::assets::AssetCatalog::ListenerId m_catalogListener = 0;
    std::mutex m_catalogEventMutex;
    std::vector<gm::assets::AssetEvent> m_catalogEvents;
    std::atomic<bool> m_catalogDirty{false};

    void RegisterCatalogListener();
    void UnregisterCatalogListener();
};

