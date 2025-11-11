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
    gm::Shader* GetShader() const { return m_shader.get(); }
    gm::Texture* GetTexture() const { return m_texture.get(); }
    gm::Mesh* GetMesh() const { return m_mesh.get(); }
    gm::Mesh* GetPlaneMesh() const { return m_planeMesh.get(); }
    gm::Mesh* GetCubeMesh() const { return m_cubeMesh.get(); }
    std::shared_ptr<gm::Material> GetPlaneMaterial() const { return m_planeMaterial; }
    std::shared_ptr<gm::Material> GetCubeMaterial() const { return m_cubeMaterial; }

    // Path accessors (for hot reload)
    const std::string& GetShaderVertPath() const { return m_shaderVertPath; }
    const std::string& GetShaderFragPath() const { return m_shaderFragPath; }
    const std::string& GetTexturePath() const { return m_texturePath; }
    const std::string& GetMeshPath() const { return m_meshPath; }

private:
    std::unique_ptr<gm::Shader> m_shader;
    std::unique_ptr<gm::Texture> m_texture;
    std::unique_ptr<gm::Mesh> m_mesh;

    std::unique_ptr<gm::Mesh> m_planeMesh;
    std::unique_ptr<gm::Mesh> m_cubeMesh;
    std::shared_ptr<gm::Material> m_planeMaterial;
    std::shared_ptr<gm::Material> m_cubeMaterial;

    std::string m_shaderGuid;
    std::string m_shaderVertPath;
    std::string m_shaderFragPath;
    std::string m_textureGuid;
    std::string m_texturePath;
    std::string m_meshGuid;
    std::string m_meshPath;
    
    std::filesystem::path m_assetsDir;
};

