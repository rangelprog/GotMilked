#include "GameResources.hpp"

#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Texture.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/rendering/Material.hpp"
#include "gm/prototypes/Primitives.hpp"
#include "gm/utils/ObjLoader.hpp"
#include "gm/utils/ResourceRegistry.hpp"
#include "gm/core/Logger.hpp"

#include <exception>
#include <filesystem>
#include <glm/vec3.hpp>

namespace {
std::shared_ptr<gm::Material> CreateMaterial(const glm::vec3& diffuse, float shininess = 32.0f) {
    auto material = std::make_shared<gm::Material>(gm::Material::CreatePhong(diffuse, glm::vec3(0.5f), shininess));
    return material;
}
} // namespace

bool GameResources::Load(const std::filesystem::path& assetsDir, 
                         const gm::utils::ResourcePathConfig& resourceConfig) {
    m_shaderGuid = "game_shader";
    m_textureGuid = "game_texture";
    m_meshGuid = "game_mesh";
    m_assetsDir = assetsDir;

    // Resolve shader paths
    std::filesystem::path shaderVertPath = resourceConfig.ResolvePath(assetsDir, resourceConfig.shaderVert);
    std::filesystem::path shaderFragPath = resourceConfig.ResolvePath(assetsDir, resourceConfig.shaderFrag);
    m_shaderVertPath = shaderVertPath.string();
    m_shaderFragPath = shaderFragPath.string();
    
    // Resolve texture paths - try ground.png first, fall back to cow.png for backward compatibility
    std::filesystem::path groundTexturePath = resourceConfig.ResolvePath(assetsDir, resourceConfig.textureGround);
    std::filesystem::path cowTexturePath = resourceConfig.ResolvePath(assetsDir, resourceConfig.textureCow);
    
    if (std::filesystem::exists(groundTexturePath)) {
        m_texturePath = groundTexturePath.string();
    } else if (std::filesystem::exists(cowTexturePath)) {
        m_texturePath = cowTexturePath.string();
        gm::core::Logger::Warning("[GameResources] Using deprecated cow.png texture. Please rename to ground.png");
    } else {
        m_texturePath = groundTexturePath.string(); // Will fail with clear error message
    }
    
    // Resolve mesh path (optional - only load if file exists)
    std::filesystem::path meshPath = resourceConfig.ResolvePath(assetsDir, resourceConfig.meshPlaceholder);
    m_meshPath = meshPath.string();

    m_shader = std::make_unique<gm::Shader>();
    if (!m_shader->loadFromFiles(m_shaderVertPath, m_shaderFragPath)) {
        gm::core::Logger::Error("[GameResources] Failed to load shader: %s / %s",
                    m_shaderVertPath.c_str(), m_shaderFragPath.c_str());
        m_shader.reset();
        return false;
    }

    m_shader->Use();
    m_shader->SetInt("uTex", 0);

    try {
        m_texture = std::make_unique<gm::Texture>(gm::Texture::loadOrDie(m_texturePath, true));
    } catch (const std::exception& ex) {
        gm::core::Logger::Error("[GameResources] Failed to load texture %s: %s",
                    m_texturePath.c_str(), ex.what());
        return false;
    } catch (...) {
        gm::core::Logger::Error("[GameResources] Failed to load texture %s: unknown error",
                    m_texturePath.c_str());
        return false;
    }
    
    // Mesh loading is optional - only load if file exists
    try {
        if (std::filesystem::exists(meshPath)) {
            m_mesh = std::make_unique<gm::Mesh>(gm::ObjLoader::LoadObjPNUV(meshPath.string()));
        } else {
            gm::core::Logger::Warning("[GameResources] Optional mesh not found: %s", 
                        meshPath.string().c_str());
            m_meshPath.clear(); // Clear path to indicate mesh is not available
        }
    } catch (const std::exception& ex) {
        gm::core::Logger::Warning("[GameResources] Optional mesh not loaded: %s (%s)", 
                    meshPath.string().c_str(), ex.what());
        m_meshPath.clear(); // Clear path to indicate mesh is not available
    }

    auto& registry = gm::ResourceRegistry::Instance();
    registry.RegisterShader(m_shaderGuid, m_shaderVertPath, m_shaderFragPath);
    registry.RegisterTexture(m_textureGuid, m_texturePath);
    if (!m_meshPath.empty()) {
        registry.RegisterMesh(m_meshGuid, m_meshPath);
    }

    m_planeMesh = std::make_unique<gm::Mesh>(gm::prototypes::CreatePlane(50.0f, 50.0f, 10.0f));
    m_cubeMesh = std::make_unique<gm::Mesh>(gm::prototypes::CreateCube(1.5f));

    m_planeMaterial = CreateMaterial(glm::vec3(0.2f, 0.5f, 0.2f), 16.0f);
    m_planeMaterial->SetName("Ground Material");
    m_planeMaterial->SetDiffuseTexture(m_texture.get());

    m_cubeMaterial = CreateMaterial(glm::vec3(0.75f, 0.2f, 0.2f));
    m_cubeMaterial->SetName("Cube Material");

    return true;
}

bool GameResources::Load(const std::string& assetsDir) {
    // Backward compatibility: use default resource paths
    gm::utils::ResourcePathConfig defaultConfig;
    return Load(std::filesystem::path(assetsDir), defaultConfig);
}

bool GameResources::ReloadShader() {
    if (m_shaderVertPath.empty() || m_shaderFragPath.empty()) {
        gm::core::Logger::Warning("[GameResources] Cannot reload shader: paths not set");
        return false;
    }

    auto newShader = std::make_unique<gm::Shader>();
    if (!newShader->loadFromFiles(m_shaderVertPath, m_shaderFragPath)) {
        gm::core::Logger::Error("[GameResources] Failed to reload shader: %s / %s",
                    m_shaderVertPath.c_str(), m_shaderFragPath.c_str());
        return false;
    }

    newShader->Use();
    newShader->SetInt("uTex", 0);
    m_shader = std::move(newShader);

    gm::ResourceRegistry::Instance().RegisterShader(m_shaderGuid, m_shaderVertPath, m_shaderFragPath);
    return true;
}

bool GameResources::ReloadTexture() {
    if (m_texturePath.empty()) {
        gm::core::Logger::Warning("[GameResources] Cannot reload texture: path not set");
        return false;
    }

    try {
        auto newTexture = std::make_unique<gm::Texture>(gm::Texture::loadOrDie(m_texturePath, true));
        m_texture = std::move(newTexture);
        if (m_planeMaterial) {
            m_planeMaterial->SetDiffuseTexture(m_texture.get());
        }
        gm::ResourceRegistry::Instance().RegisterTexture(m_textureGuid, m_texturePath);
        return true;
    } catch (const std::exception& ex) {
        gm::core::Logger::Error("[GameResources] Failed to reload texture %s: %s",
                    m_texturePath.c_str(), ex.what());
    } catch (...) {
        gm::core::Logger::Error("[GameResources] Failed to reload texture %s: unknown error",
                    m_texturePath.c_str());
    }
    return false;
}

bool GameResources::ReloadMesh() {
    if (m_meshPath.empty()) {
        gm::core::Logger::Warning("[GameResources] Cannot reload mesh: path not set");
        return false;
    }

    try {
        auto newMesh = std::make_unique<gm::Mesh>(gm::ObjLoader::LoadObjPNUV(m_meshPath));
        m_mesh = std::move(newMesh);
        gm::ResourceRegistry::Instance().RegisterMesh(m_meshGuid, m_meshPath);
        return true;
    } catch (const std::exception& ex) {
        gm::core::Logger::Error("[GameResources] Failed to reload mesh %s: %s",
                    m_meshPath.c_str(), ex.what());
    } catch (...) {
        gm::core::Logger::Error("[GameResources] Failed to reload mesh %s: unknown error",
                    m_meshPath.c_str());
    }
    return false;
}

bool GameResources::ReloadAll() {
    bool shaderOk = ReloadShader();
    bool textureOk = ReloadTexture();
    bool meshOk = ReloadMesh();
    
    if (!shaderOk) {
        gm::core::Logger::Error("[GameResources] ReloadAll: shader reload failed");
    }
    if (!textureOk) {
        gm::core::Logger::Error("[GameResources] ReloadAll: texture reload failed");
    }
    if (!meshOk) {
        gm::core::Logger::Error("[GameResources] ReloadAll: mesh reload failed");
    }
    
    bool allOk = shaderOk && textureOk && meshOk;
    if (allOk) {
        gm::core::Logger::Info("[GameResources] ReloadAll: all resources reloaded successfully");
    }
    
    return allOk;
}

void GameResources::Release() {
    auto& registry = gm::ResourceRegistry::Instance();
    registry.UnregisterShader(m_shaderGuid);
    registry.UnregisterTexture(m_textureGuid);
    if (!m_meshGuid.empty()) {
        registry.UnregisterMesh(m_meshGuid);
    }

    m_shader.reset();
    m_texture.reset();
    m_mesh.reset();
    m_planeMesh.reset();
    m_cubeMesh.reset();
    m_planeMaterial.reset();
    m_cubeMaterial.reset();
    m_shaderGuid.clear();
    m_shaderVertPath.clear();
    m_shaderFragPath.clear();
    m_textureGuid.clear();
    m_texturePath.clear();
    m_meshGuid.clear();
    m_meshPath.clear();
}

