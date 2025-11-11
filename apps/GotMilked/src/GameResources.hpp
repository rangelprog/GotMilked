#pragma once

#include <memory>
#include <string>
#include <filesystem>

#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Texture.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/rendering/Material.hpp"
#include "gm/utils/Config.hpp"

// Forward declaration for test helper (defined in tests)
struct TestAssetBundle;
void PopulateGameResourcesFromTestAssets(const TestAssetBundle& bundle, class GameResources& resources);

class GameResources {
    // Friend declaration for test helper
    friend void PopulateGameResourcesFromTestAssets(const TestAssetBundle&, GameResources&);

public:
    /**
     * @brief Load resources using the provided configuration.
     * @param assetsDir Base assets directory
     * @param resourceConfig Resource path configuration
     * @return true if loading succeeded, false otherwise
     */
    bool Load(const std::filesystem::path& assetsDir, 
              const gm::utils::ResourcePathConfig& resourceConfig);
    
    /**
     * @brief Load resources using default paths (backward compatibility).
     * @param assetsDir Base assets directory
     * @return true if loading succeeded, false otherwise
     */
    bool Load(const std::string& assetsDir);
    bool ReloadShader();
    bool ReloadTexture();
    bool ReloadMesh();
    bool ReloadAll();
    void Release();

    // Resource accessors
    gm::Shader* GetShader() const { return m_shader ? m_shader.get() : nullptr; }
    gm::Texture* GetTexture() const { return m_texture ? m_texture.get() : nullptr; }
    gm::Mesh* GetMesh() const { return m_mesh ? m_mesh.get() : nullptr; }
    std::shared_ptr<gm::Material> GetTerrainMaterial() const { return m_terrainMaterial; }

    // Path accessors (for hot reload)
    const std::string& GetShaderVertPath() const { return m_shaderVertPath; }
    const std::string& GetShaderFragPath() const { return m_shaderFragPath; }
    const std::string& GetTexturePath() const { return m_texturePath; }
    const std::string& GetMeshPath() const { return m_meshPath; }

    // GUID accessors (for resource restoration)
    const std::string& GetShaderGuid() const { return m_shaderGuid; }
    const std::string& GetTextureGuid() const { return m_textureGuid; }
    const std::string& GetMeshGuid() const { return m_meshGuid; }

private:
    std::shared_ptr<gm::Shader> m_shader;
    std::shared_ptr<gm::Texture> m_texture;
    std::shared_ptr<gm::Mesh> m_mesh;
    std::shared_ptr<gm::Material> m_terrainMaterial;

    std::string m_shaderGuid;
    std::string m_shaderVertPath;
    std::string m_shaderFragPath;
    std::string m_textureGuid;
    std::string m_texturePath;
    std::string m_meshGuid;
    std::string m_meshPath;
    
    std::filesystem::path m_assetsDir;
};

