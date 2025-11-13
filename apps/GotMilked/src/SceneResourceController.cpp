#include "SceneResourceController.hpp"

#include "Game.hpp"
#include "GameResources.hpp"
#include "ToolingFacade.hpp"
#include "gm/core/Logger.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/scene/StaticMeshComponent.hpp"
#include "gm/rendering/Camera.hpp"
#include "gm/rendering/Material.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/rendering/Shader.hpp"

#include <fmt/format.h>

#if GM_DEBUG_TOOLS
#include "DebugHudController.hpp"
#include "EditableTerrainComponent.hpp"
#include "gm/debug/GridRenderer.hpp"
#include "gm/rendering/Texture.hpp"
#endif

#include <algorithm>
#include <unordered_set>

#if GM_DEBUG_TOOLS
using gm::debug::EditableTerrainComponent;
#endif

namespace {

void ReportSceneIssue(Game& game, const std::string& message, bool isError) {
    ToolingFacade* tooling = game.GetToolingFacade();
    const bool hasTooling = tooling != nullptr;
    if (isError) {
        if (hasTooling) {
            gm::core::Logger::Debug("[SceneResources] {}", message);
        } else {
            gm::core::Logger::Error("[SceneResources] {}", message);
        }
    } else {
        if (hasTooling) {
            gm::core::Logger::Debug("[SceneResources] {}", message);
        } else {
            gm::core::Logger::Warning("[SceneResources] {}", message);
        }
    }

    if (hasTooling) {
        const std::string formatted = fmt::format("Scene resource {}: {}",
                                                  isError ? "error" : "warning",
                                                  message);
        tooling->AddNotification(formatted);
    }
}

} // namespace

SceneResourceController::SceneResourceController(Game& game)
    : m_game(game) {}

void SceneResourceController::ApplyResourcesToScene() {
    if (!m_game.m_gameScene) {
        gm::core::Logger::Warning("[Game] ApplyResourcesToScene: No scene available");
        return;
    }

#if GM_DEBUG_TOOLS
    ApplyResourcesToTerrain();
#endif
    ApplyResourcesToStaticMeshComponents();

    if (auto* tooling = m_game.GetToolingFacade()) {
        tooling->UpdateSceneReference();
        tooling->RefreshHud();
    }
    if (m_game.m_gameScene) {
        m_game.m_gameScene->InvalidateInstancedGroups();
    }
}

void SceneResourceController::ApplyResourcesToStaticMeshComponents() {
    if (!m_game.m_gameScene) {
        return;
    }

    ClearStaticMeshDependencies();

    for (const auto& gameObject : m_game.m_gameScene->GetAllGameObjects()) {
        if (!gameObject || !gameObject->IsActive()) {
            continue;
        }

        auto meshCompShared = gameObject->GetComponent<gm::scene::StaticMeshComponent>();
        if (!meshCompShared) {
            continue;
        }

        ResolveStaticMeshComponent(gameObject, meshCompShared.get());
    }
}

void SceneResourceController::RefreshShaders(const std::vector<std::string>& guids) {
    std::unordered_set<gm::scene::StaticMeshComponent*> candidates;
    for (const auto& guid : guids) {
        std::string key = m_game.m_resources.ResolveShaderAlias(guid);
        auto it = m_shaderDependents.find(key);
        if (it != m_shaderDependents.end()) {
            candidates.insert(it->second.begin(), it->second.end());
        }
    }
    for (auto* component : candidates) {
        ResolveStaticMeshComponentBinding(component);
    }
}

void SceneResourceController::RefreshMeshes(const std::vector<std::string>& guids) {
    std::unordered_set<gm::scene::StaticMeshComponent*> candidates;
    for (const auto& guid : guids) {
        std::string key = m_game.m_resources.ResolveMeshAlias(guid);
        auto it = m_meshDependents.find(key);
        if (it != m_meshDependents.end()) {
            candidates.insert(it->second.begin(), it->second.end());
        }
    }
    for (auto* component : candidates) {
        ResolveStaticMeshComponentBinding(component);
    }
}

void SceneResourceController::RefreshMaterials(const std::vector<std::string>& guids) {
    std::unordered_set<gm::scene::StaticMeshComponent*> candidates;
    for (const auto& guid : guids) {
        std::string key = m_game.m_resources.ResolveMaterialAlias(guid);
        auto it = m_materialDependents.find(key);
        if (it != m_materialDependents.end()) {
            candidates.insert(it->second.begin(), it->second.end());
        }
    }
    for (auto* component : candidates) {
        ResolveStaticMeshComponentBinding(component);
    }
}

#if GM_DEBUG_TOOLS
void SceneResourceController::ApplyResourcesToTerrain() {
    if (!m_game.m_gameScene) {
        return;
    }

    auto terrainObject = m_game.m_gameScene->FindGameObjectByName("Terrain");
    if (!terrainObject) {
        return;
    }

    auto terrain = terrainObject->GetComponent<EditableTerrainComponent>();
    if (!terrain) {
        return;
    }

    terrain->SetShader(m_game.m_resources.GetShader());
    terrain->SetMaterial(m_game.m_resources.GetTerrainMaterial());

    const auto& textureMap = m_game.m_resources.GetTextureMap();
    std::string baseTextureGuid = terrain->GetBaseTextureGuid();
    if (!baseTextureGuid.empty()) {
        std::shared_ptr<gm::Texture> textureShared;
        if (auto it = textureMap.find(baseTextureGuid); it != textureMap.end() && it->second) {
            textureShared = it->second;
        } else {
            textureShared = m_game.m_resources.EnsureTextureAvailable(baseTextureGuid);
        }
        if (textureShared) {
            terrain->BindBaseTexture(textureShared.get());
        }
    }

    for (int layer = 0; layer < terrain->GetPaintLayerCount(); ++layer) {
        std::string layerGuid = terrain->GetPaintTextureGuid(layer);
        if (layerGuid.empty()) {
            continue;
        }
        std::shared_ptr<gm::Texture> textureShared;
        if (auto it = textureMap.find(layerGuid); it != textureMap.end() && it->second) {
            textureShared = it->second;
        } else {
            textureShared = m_game.m_resources.EnsureTextureAvailable(layerGuid);
        }
        if (textureShared) {
            terrain->BindPaintTexture(layer, textureShared.get());
        }
    }

    terrainObject->Init();
    terrain->Init();
    terrain->MarkMeshDirty();

    if (auto* tooling = m_game.GetToolingFacade()) {
        tooling->RegisterTerrain(terrain.get());
    }
    if (m_game.m_terrainEditingSystem) {
        m_game.m_terrainEditingSystem->RefreshBindings();
    }
}
#endif

void SceneResourceController::ClearStaticMeshDependencies() {
    m_staticMeshBindings.clear();
    m_meshDependents.clear();
    m_shaderDependents.clear();
    m_materialDependents.clear();
}

void SceneResourceController::EraseDependent(
    std::unordered_map<std::string, std::unordered_set<gm::scene::StaticMeshComponent*>>& map,
    const std::string& guid,
    gm::scene::StaticMeshComponent* component) {
    if (guid.empty()) {
        return;
    }
    auto it = map.find(guid);
    if (it == map.end()) {
        return;
    }
    it->second.erase(component);
    if (it->second.empty()) {
        map.erase(it);
    }
}

void SceneResourceController::RemoveStaticMeshBinding(gm::scene::StaticMeshComponent* component) {
    auto it = m_staticMeshBindings.find(component);
    if (it == m_staticMeshBindings.end()) {
        return;
    }
    EraseDependent(m_meshDependents, it->second.meshGuid, component);
    EraseDependent(m_shaderDependents, it->second.shaderGuid, component);
    EraseDependent(m_materialDependents, it->second.materialGuid, component);
    m_staticMeshBindings.erase(it);
}

void SceneResourceController::RegisterStaticMeshBinding(gm::scene::StaticMeshComponent* component,
                                                        const std::shared_ptr<gm::GameObject>& owner,
                                                        const std::string& meshGuid,
                                                        const std::string& shaderGuid,
                                                        const std::string& materialGuid) {
    if (!component) {
        return;
    }

    RemoveStaticMeshBinding(component);

    StaticMeshBinding binding;
    binding.owner = owner;
    binding.meshGuid = meshGuid;
    binding.shaderGuid = shaderGuid;
    binding.materialGuid = materialGuid;

    m_staticMeshBindings[component] = binding;

    if (!meshGuid.empty()) {
        m_meshDependents[meshGuid].insert(component);
    }
    if (!shaderGuid.empty()) {
        m_shaderDependents[shaderGuid].insert(component);
    }
    if (!materialGuid.empty()) {
        m_materialDependents[materialGuid].insert(component);
    }
}

void SceneResourceController::ResolveStaticMeshComponent(const std::shared_ptr<gm::GameObject>& gameObject,
                                                         gm::scene::StaticMeshComponent* meshComp) {
    if (!meshComp || !gameObject) {
        return;
    }

    RemoveStaticMeshBinding(meshComp);

    bool updatedAny = false;

    const std::string originalMeshGuid = meshComp->GetMeshGuid();
    const std::string meshGuid = m_game.m_resources.ResolveMeshAlias(originalMeshGuid);
    gm::Mesh* resolvedMesh = nullptr;
    if (!meshGuid.empty()) {
        resolvedMesh = m_game.m_resources.GetMesh(meshGuid);
    }
    if (resolvedMesh) {
        if (resolvedMesh != meshComp->GetMesh() || meshComp->GetMeshGuid() != meshGuid) {
            meshComp->SetMesh(resolvedMesh, meshGuid);
            updatedAny = true;
        }
    } else if (!meshGuid.empty()) {
        meshComp->SetMesh(nullptr, std::string());
        ReportSceneIssue(
            m_game,
            fmt::format("[Game] StaticMeshComponent on '{}' references missing mesh GUID '{}'",
                        gameObject->GetName(), originalMeshGuid),
            true);
    }

    const std::string originalShaderGuid = meshComp->GetShaderGuid();
    const std::string shaderGuid = m_game.m_resources.ResolveShaderAlias(originalShaderGuid);
    gm::Shader* resolvedShader = nullptr;
    if (!shaderGuid.empty()) {
        resolvedShader = m_game.m_resources.GetShader(shaderGuid);
    }
    if (resolvedShader) {
        if (resolvedShader != meshComp->GetShader() || meshComp->GetShaderGuid() != shaderGuid) {
            meshComp->SetShader(resolvedShader, shaderGuid);
            resolvedShader->Use();
            resolvedShader->SetInt("uTex", 0);
            updatedAny = true;
        }
    } else if (!shaderGuid.empty()) {
        meshComp->SetShader(nullptr, std::string());
        ReportSceneIssue(
            m_game,
            fmt::format("[Game] StaticMeshComponent on '{}' references missing shader GUID '{}'",
                        gameObject->GetName(), originalShaderGuid),
            true);
    }

    const std::string originalMaterialGuid = meshComp->GetMaterialGuid();
    const std::string materialGuid = m_game.m_resources.ResolveMaterialAlias(originalMaterialGuid);
    std::shared_ptr<gm::Material> resolvedMaterial;
    if (!materialGuid.empty()) {
        resolvedMaterial = m_game.m_resources.GetMaterial(materialGuid);
    }
    if (resolvedMaterial) {
        auto currentMaterial = meshComp->GetMaterial();
        if (!currentMaterial || currentMaterial.get() != resolvedMaterial.get() || meshComp->GetMaterialGuid() != materialGuid) {
            meshComp->SetMaterial(resolvedMaterial, materialGuid);
            updatedAny = true;
        }
    } else if (!materialGuid.empty()) {
        meshComp->SetMaterial(nullptr, std::string());
        ReportSceneIssue(
            m_game,
            fmt::format("[Game] StaticMeshComponent on '{}' references missing material GUID '{}'",
                        gameObject->GetName(), originalMaterialGuid),
            true);
    }

    RegisterStaticMeshBinding(meshComp, gameObject, meshGuid, shaderGuid, materialGuid);

    if (updatedAny) {
        gm::core::Logger::Info("[Game] Updated resources for StaticMeshComponent on GameObject '{}'",
                               gameObject->GetName());
    }
}

void SceneResourceController::ResolveStaticMeshComponentBinding(gm::scene::StaticMeshComponent* component) {
    if (!component) {
        return;
    }

    auto it = m_staticMeshBindings.find(component);
    if (it == m_staticMeshBindings.end()) {
        return;
    }

    auto owner = it->second.owner.lock();
    if (!owner) {
        RemoveStaticMeshBinding(component);
        return;
    }

    ResolveStaticMeshComponent(owner, component);
}

