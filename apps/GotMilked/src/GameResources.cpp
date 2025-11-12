#include "GameResources.hpp"
#include "GameConstants.hpp"
#include "GameEvents.hpp"

#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Texture.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/rendering/Material.hpp"
#include "gm/prototypes/Primitives.hpp"
#include "gm/utils/ObjLoader.hpp"
#include "gm/utils/ResourceRegistry.hpp"
#include "gm/utils/ResourceManager.hpp"
#include "gm/core/Logger.hpp"
#include "gm/core/Event.hpp"

#include <exception>
#include <filesystem>
#include <glm/vec3.hpp>

namespace {
std::shared_ptr<gm::Material> CreateMaterial(const glm::vec3& diffuse, float shininess = gotmilked::GameConstants::Material::DefaultShininess) {
    auto material = std::make_shared<gm::Material>(gm::Material::CreatePhong(
        diffuse, 
        gotmilked::GameConstants::Material::DefaultSpecular, 
        shininess));
    return material;
}

std::shared_ptr<gm::core::Error> CloneError(const gm::core::Error& err) {
    if (auto resourceErr = dynamic_cast<const gm::core::ResourceError*>(&err)) {
        return std::make_shared<gm::core::ResourceError>(*resourceErr);
    }
    if (auto graphicsErr = dynamic_cast<const gm::core::GraphicsError*>(&err)) {
        return std::make_shared<gm::core::GraphicsError>(*graphicsErr);
    }
    return std::make_shared<gm::core::Error>(err.what());
}
} // namespace

bool GameResources::Load(const std::filesystem::path& assetsDir, 
                         const gm::utils::ResourcePathConfig& resourceConfig) {
    m_lastError.reset();
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

    // Load shader through ResourceManager (provides caching and sharing)
    try {
        m_shader = gm::ResourceManager::LoadShader(m_shaderGuid, m_shaderVertPath, m_shaderFragPath);
        m_shader->Use();
        m_shader->SetInt("uTex", 0);
        gm::core::Event::Trigger(gotmilked::GameEvents::ResourceShaderLoaded);

        // Load texture through ResourceManager (provides caching and sharing)
        m_texture = gm::ResourceManager::LoadTexture(m_textureGuid, m_texturePath);
        gm::core::Event::Trigger(gotmilked::GameEvents::ResourceTextureLoaded);
    } catch (const gm::core::Error& err) {
        StoreError(err);
        gm::core::Logger::Error("[GameResources] {}", err.what());
        gm::core::Event::Trigger(gotmilked::GameEvents::ResourceLoadFailed);
        gm::core::Event::TriggerWithData(gotmilked::GameEvents::ResourceLoadFailed, m_lastError.get());
        return false;
    } catch (const std::exception& ex) {
        gm::core::Error generic(std::string("GameResources load failed: ") + ex.what());
        StoreError(generic);
        gm::core::Logger::Error("[GameResources] {}", generic.what());
        gm::core::Event::Trigger(gotmilked::GameEvents::ResourceLoadFailed);
        gm::core::Event::TriggerWithData(gotmilked::GameEvents::ResourceLoadFailed, m_lastError.get());
        return false;
    }

    // Mesh loading is optional - only load if file exists
    if (std::filesystem::exists(meshPath)) {
        try {
            m_mesh = gm::ResourceManager::LoadMesh(m_meshGuid, m_meshPath);
            gm::core::Event::Trigger(gotmilked::GameEvents::ResourceMeshLoaded);
        } catch (const gm::core::ResourceError& err) {
            gm::core::Logger::Warning("[GameResources] Optional mesh not loaded: {}", err.what());
            m_meshPath.clear();
        }
    } else {
        gm::core::Logger::Warning("[GameResources] Optional mesh not found: {}", m_meshPath);
        m_meshPath.clear(); // Clear path to indicate mesh is not available
    }

    auto& registry = gm::ResourceRegistry::Instance();
    registry.RegisterShader(m_shaderGuid, m_shaderVertPath, m_shaderFragPath);
    registry.RegisterTexture(m_textureGuid, m_texturePath);
    if (!m_meshPath.empty()) {
        registry.RegisterMesh(m_meshGuid, m_meshPath);
    }

    // Create terrain material
    m_terrainMaterial = CreateMaterial(
        gotmilked::GameConstants::Material::PlaneDiffuse,
        gotmilked::GameConstants::Material::PlaneShininess);
    m_terrainMaterial->SetName("Terrain Material");
    m_terrainMaterial->SetDiffuseTexture(m_texture.get());

    return true;
}

void GameResources::StoreError(const gm::core::Error& err) {
    m_lastError = CloneError(err);
}

bool GameResources::Load(const std::string& assetsDir) {
    // Backward compatibility: use default resource paths
    gm::utils::ResourcePathConfig defaultConfig;
    return Load(std::filesystem::path(assetsDir), defaultConfig);
}

bool GameResources::ReloadShader() {
    if (m_shaderVertPath.empty() || m_shaderFragPath.empty()) {
        gm::core::Error err("GameResources: shader paths not set");
        StoreError(err);
        gm::core::Logger::Warning("[GameResources] Cannot reload shader: paths not set");
        return false;
    }

    try {
        auto reloadedShader = gm::ResourceManager::ReloadShader(m_shaderGuid, m_shaderVertPath, m_shaderFragPath);
        reloadedShader->Use();
        reloadedShader->SetInt("uTex", 0);
        m_shader = reloadedShader;

        gm::ResourceRegistry::Instance().RegisterShader(m_shaderGuid, m_shaderVertPath, m_shaderFragPath);
        gm::core::Event::Trigger(gotmilked::GameEvents::ResourceShaderReloaded);
        return true;
    } catch (const gm::core::Error& err) {
        StoreError(err);
        gm::core::Logger::Error("[GameResources] {}", err.what());
        gm::core::Event::Trigger(gotmilked::GameEvents::ResourceLoadFailed);
        gm::core::Event::TriggerWithData(gotmilked::GameEvents::ResourceLoadFailed, m_lastError.get());
        return false;
    } catch (const std::exception& ex) {
        gm::core::Error err(std::string("GameResources shader reload failed: ") + ex.what());
        StoreError(err);
        gm::core::Logger::Error("[GameResources] {}", err.what());
        gm::core::Event::Trigger(gotmilked::GameEvents::ResourceLoadFailed);
        gm::core::Event::TriggerWithData(gotmilked::GameEvents::ResourceLoadFailed, m_lastError.get());
        return false;
    }
}

bool GameResources::ReloadTexture() {
    if (m_texturePath.empty()) {
        gm::core::Error err("GameResources: texture path not set");
        StoreError(err);
        gm::core::Logger::Warning("[GameResources] Cannot reload texture: path not set");
        return false;
    }

    try {
        auto reloadedTexture = gm::ResourceManager::ReloadTexture(m_textureGuid, m_texturePath);
        m_texture = reloadedTexture;
        if (m_terrainMaterial) {
            m_terrainMaterial->SetDiffuseTexture(m_texture.get());
        }

        gm::ResourceRegistry::Instance().RegisterTexture(m_textureGuid, m_texturePath);
        gm::core::Event::Trigger(gotmilked::GameEvents::ResourceTextureReloaded);
        return true;
    } catch (const gm::core::Error& err) {
        StoreError(err);
        gm::core::Logger::Error("[GameResources] {}", err.what());
        gm::core::Event::Trigger(gotmilked::GameEvents::ResourceLoadFailed);
        gm::core::Event::TriggerWithData(gotmilked::GameEvents::ResourceLoadFailed, m_lastError.get());
        return false;
    } catch (const std::exception& ex) {
        gm::core::Error err(std::string("GameResources texture reload failed: ") + ex.what());
        StoreError(err);
        gm::core::Logger::Error("[GameResources] {}", err.what());
        gm::core::Event::Trigger(gotmilked::GameEvents::ResourceLoadFailed);
        gm::core::Event::TriggerWithData(gotmilked::GameEvents::ResourceLoadFailed, m_lastError.get());
        return false;
    }
}

bool GameResources::ReloadMesh() {
    if (m_meshPath.empty()) {
        gm::core::Error err("GameResources: mesh path not set");
        StoreError(err);
        gm::core::Logger::Warning("[GameResources] Cannot reload mesh: path not set");
        return false;
    }

    try {
        auto reloadedMesh = gm::ResourceManager::ReloadMesh(m_meshGuid, m_meshPath);
        m_mesh = reloadedMesh;

        gm::ResourceRegistry::Instance().RegisterMesh(m_meshGuid, m_meshPath);
        gm::core::Event::Trigger(gotmilked::GameEvents::ResourceMeshReloaded);
        return true;
    } catch (const gm::core::Error& err) {
        StoreError(err);
        gm::core::Logger::Error("[GameResources] {}", err.what());
        gm::core::Event::Trigger(gotmilked::GameEvents::ResourceLoadFailed);
        gm::core::Event::TriggerWithData(gotmilked::GameEvents::ResourceLoadFailed, m_lastError.get());
        return false;
    } catch (const std::exception& ex) {
        gm::core::Error err(std::string("GameResources mesh reload failed: ") + ex.what());
        StoreError(err);
        gm::core::Logger::Error("[GameResources] {}", err.what());
        gm::core::Event::Trigger(gotmilked::GameEvents::ResourceLoadFailed);
        gm::core::Event::TriggerWithData(gotmilked::GameEvents::ResourceLoadFailed, m_lastError.get());
        return false;
    }
}

bool GameResources::ReloadAll() {
    bool shaderOk = ReloadShader();
    bool textureOk = ReloadTexture();
    bool meshOk = ReloadMesh();

    bool allOk = shaderOk && textureOk && meshOk;
    if (allOk) {
        gm::core::Logger::Info("[GameResources] ReloadAll: all resources reloaded successfully");
        gm::core::Event::Trigger(gotmilked::GameEvents::ResourceAllReloaded);
    }

    return allOk;
}

void GameResources::Release() {
    // Unregister from ResourceRegistry (GUID tracking for serialization)
    auto& registry = gm::ResourceRegistry::Instance();
    registry.UnregisterShader(m_shaderGuid);
    registry.UnregisterTexture(m_textureGuid);
    if (!m_meshGuid.empty()) {
        registry.UnregisterMesh(m_meshGuid);
    }

    // Release local references (ResourceManager still holds shared_ptr if other code uses them)
    m_shader.reset();
    m_texture.reset();
    m_mesh.reset();
    m_terrainMaterial.reset();
    m_shaderGuid.clear();
    m_shaderVertPath.clear();
    m_shaderFragPath.clear();
    m_textureGuid.clear();
    m_texturePath.clear();
    m_meshGuid.clear();
    m_meshPath.clear();
    m_lastError.reset();
}

