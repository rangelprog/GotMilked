#include "SceneResourceController.hpp"

#include "Game.hpp"
#include "GameResources.hpp"
#include "ToolingFacade.hpp"
#include "gm/core/Logger.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/scene/StaticMeshComponent.hpp"
#include "gm/scene/SkinnedMeshComponent.hpp"
#include "gm/scene/AnimatorComponent.hpp"
#include "gm/rendering/Camera.hpp"
#include "gm/rendering/Material.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Texture.hpp"
#include "gm/utils/ResourceManager.hpp"

#include <fmt/format.h>

#if GM_DEBUG_TOOLS
#include "DebugHudController.hpp"
#include "EditableTerrainComponent.hpp"
#include "gm/debug/GridRenderer.hpp"
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
    ApplyResourcesToSkinnedMeshComponents();
    ApplyResourcesToAnimatorComponents();

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

void SceneResourceController::ApplyResourcesToSkinnedMeshComponents() {
    if (!m_game.m_gameScene) {
        return;
    }

    ClearSkinnedMeshDependencies();

    for (const auto& gameObject : m_game.m_gameScene->GetAllGameObjects()) {
        if (!gameObject || !gameObject->IsActive()) {
            continue;
        }

        auto component = gameObject->GetComponent<gm::scene::SkinnedMeshComponent>();
        if (!component) {
            continue;
        }

        ResolveSkinnedMeshComponent(gameObject, component.get());
    }
}

void SceneResourceController::ApplyResourcesToAnimatorComponents() {
    if (!m_game.m_gameScene) {
        return;
    }

    for (const auto& gameObject : m_game.m_gameScene->GetAllGameObjects()) {
        if (!gameObject || !gameObject->IsActive()) {
            continue;
        }

        auto component = gameObject->GetComponent<gm::scene::AnimatorComponent>();
        if (!component) {
            continue;
        }

        ResolveAnimatorComponent(gameObject, component.get());
    }
}

void SceneResourceController::RefreshShaders(const std::vector<std::string>& guids) {
    std::unordered_set<gm::scene::StaticMeshComponent*> candidates;
    std::unordered_set<gm::scene::SkinnedMeshComponent*> skinnedCandidates;
    for (const auto& guid : guids) {
        auto it = m_shaderDependents.find(guid);
        if (it != m_shaderDependents.end()) {
            candidates.insert(it->second.begin(), it->second.end());
        }
        auto skinnedIt = m_skinnedShaderDependents.find(guid);
        if (skinnedIt != m_skinnedShaderDependents.end()) {
            skinnedCandidates.insert(skinnedIt->second.begin(), skinnedIt->second.end());
        }
    }
    for (auto* component : candidates) {
        ResolveStaticMeshComponentBinding(component);
    }
    for (auto* component : skinnedCandidates) {
        ResolveSkinnedMeshComponentBinding(component);
    }
}

void SceneResourceController::RefreshMeshes(const std::vector<std::string>& guids) {
    std::unordered_set<gm::scene::StaticMeshComponent*> candidates;
    std::unordered_set<gm::scene::SkinnedMeshComponent*> skinnedCandidates;
    for (const auto& guid : guids) {
        auto it = m_meshDependents.find(guid);
        if (it != m_meshDependents.end()) {
            candidates.insert(it->second.begin(), it->second.end());
        }
        auto skinnedIt = m_skinnedMeshDependents.find(guid);
        if (skinnedIt != m_skinnedMeshDependents.end()) {
            skinnedCandidates.insert(skinnedIt->second.begin(), skinnedIt->second.end());
        }
    }
    for (auto* component : candidates) {
        ResolveStaticMeshComponentBinding(component);
    }
    for (auto* component : skinnedCandidates) {
        ResolveSkinnedMeshComponentBinding(component);
    }
}

void SceneResourceController::RefreshMaterials(const std::vector<std::string>& guids) {
    std::unordered_set<gm::scene::StaticMeshComponent*> candidates;
    std::unordered_set<gm::scene::SkinnedMeshComponent*> skinnedCandidates;
    for (const auto& guid : guids) {
        auto it = m_materialDependents.find(guid);
        if (it != m_materialDependents.end()) {
            candidates.insert(it->second.begin(), it->second.end());
        }
        auto skinnedIt = m_skinnedMaterialDependents.find(guid);
        if (skinnedIt != m_skinnedMaterialDependents.end()) {
            skinnedCandidates.insert(skinnedIt->second.begin(), skinnedIt->second.end());
        }
    }
    for (auto* component : candidates) {
        ResolveStaticMeshComponentBinding(component);
    }
    for (auto* component : skinnedCandidates) {
        ResolveSkinnedMeshComponentBinding(component);
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

    {
        const std::string& shaderGuid = m_game.m_resources.GetShaderGuid();
        terrain->SetShader(m_game.m_resources.GetShader());
        gm::core::Logger::Info("[SceneResources] Terrain shader set to '{}'",
            shaderGuid.empty() ? "<unset>" : shaderGuid);
    }
    terrain->SetMaterial(m_game.m_resources.GetTerrainMaterial());

    const auto& textureMap = m_game.m_resources.GetTextureMap();
    std::string baseTextureGuid = terrain->GetBaseTextureGuid();
    if (baseTextureGuid.empty()) {
        gm::Texture* defaultTexture = m_game.m_resources.GetTexture();
        const std::string& defaultGuid = m_game.m_resources.GetTextureGuid();
        if (defaultTexture && !defaultGuid.empty()) {
            terrain->SetBaseTexture(defaultGuid, defaultTexture);
            baseTextureGuid = defaultGuid;
            gm::core::Logger::Info("[SceneResources] Terrain base texture fallback to default '{}'", defaultGuid);
        }
    }
    if (!baseTextureGuid.empty()) {
        std::shared_ptr<gm::Texture> textureShared;
        if (auto it = textureMap.find(baseTextureGuid); it != textureMap.end() && it->second) {
            textureShared = it->second;
        } else {
            textureShared = m_game.m_resources.EnsureTextureAvailable(baseTextureGuid);
        }
        if (textureShared) {
            terrain->BindBaseTexture(textureShared.get());
            gm::core::Logger::Info("[SceneResources] Terrain base texture set to '{}'", baseTextureGuid);
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

void SceneResourceController::ClearSkinnedMeshDependencies() {
    m_skinnedMeshBindings.clear();
    m_skinnedMeshDependents.clear();
    m_skinnedShaderDependents.clear();
    m_skinnedMaterialDependents.clear();
    m_skinnedTextureDependents.clear();
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

void SceneResourceController::EraseSkinnedDependent(
    std::unordered_map<std::string, std::unordered_set<gm::scene::SkinnedMeshComponent*>>& map,
    const std::string& guid,
    gm::scene::SkinnedMeshComponent* component) {
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
    const std::string meshGuid = originalMeshGuid;
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
    const std::string shaderGuid = originalShaderGuid;
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

    const std::string materialGuid = meshComp->GetMaterialGuid();
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
                        gameObject->GetName(), materialGuid),
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

void SceneResourceController::RemoveSkinnedMeshBinding(gm::scene::SkinnedMeshComponent* component) {
    auto it = m_skinnedMeshBindings.find(component);
    if (it == m_skinnedMeshBindings.end()) {
        return;
    }

    EraseSkinnedDependent(m_skinnedMeshDependents, it->second.meshGuid, component);
    EraseSkinnedDependent(m_skinnedShaderDependents, it->second.shaderGuid, component);
    EraseSkinnedDependent(m_skinnedMaterialDependents, it->second.materialGuid, component);
    EraseSkinnedDependent(m_skinnedTextureDependents, it->second.textureGuid, component);
    m_skinnedMeshBindings.erase(it);
}

void SceneResourceController::RegisterSkinnedMeshBinding(gm::scene::SkinnedMeshComponent* component,
                                                         const std::shared_ptr<gm::GameObject>& owner,
                                                         const std::string& meshGuid,
                                                         const std::string& shaderGuid,
                                                         const std::string& materialGuid,
                                                         const std::string& textureGuid) {
    if (!component) {
        return;
    }

    RemoveSkinnedMeshBinding(component);

    SkinnedMeshBinding binding;
    binding.owner = owner;
    binding.meshGuid = meshGuid;
    binding.shaderGuid = shaderGuid;
    binding.materialGuid = materialGuid;
    binding.textureGuid = textureGuid;

    m_skinnedMeshBindings[component] = binding;

    if (!meshGuid.empty()) {
        m_skinnedMeshDependents[meshGuid].insert(component);
    }
    if (!shaderGuid.empty()) {
        m_skinnedShaderDependents[shaderGuid].insert(component);
    }
    if (!materialGuid.empty()) {
        m_skinnedMaterialDependents[materialGuid].insert(component);
    }
    if (!textureGuid.empty()) {
        m_skinnedTextureDependents[textureGuid].insert(component);
    }
}

void SceneResourceController::ResolveSkinnedMeshComponentBinding(gm::scene::SkinnedMeshComponent* component) {
    if (!component) {
        return;
    }

    auto it = m_skinnedMeshBindings.find(component);
    if (it == m_skinnedMeshBindings.end()) {
        return;
    }

    auto owner = it->second.owner.lock();
    if (!owner) {
        RemoveSkinnedMeshBinding(component);
        return;
    }

    ResolveSkinnedMeshComponent(owner, component);
}

void SceneResourceController::ResolveSkinnedMeshComponent(const std::shared_ptr<gm::GameObject>& gameObject,
                                                          gm::scene::SkinnedMeshComponent* component) {
    if (!component || !gameObject) {
        return;
    }

    RemoveSkinnedMeshBinding(component);

    const std::string meshGuid = component->MeshGuid();
    if (!meshGuid.empty()) {
        if (auto meshPath = m_game.m_resources.GetSkinnedMeshPath(meshGuid)) {
            try {
                gm::ResourceManager::SkinnedMeshDescriptor desc{meshGuid, *meshPath};
                auto handle = gm::ResourceManager::LoadSkinnedMesh(desc);
                component->SetMesh(std::move(handle));
            } catch (const std::exception& ex) {
                ReportSceneIssue(
                    m_game,
                    fmt::format("Failed to load skinned mesh '{}' for '{}': {}", meshGuid, gameObject->GetName(), ex.what()),
                    true);
            }
        } else {
            ReportSceneIssue(
                m_game,
                fmt::format("Skinned mesh GUID '{}' referenced by '{}' has no registered asset path",
                            meshGuid,
                            gameObject->GetName()),
                true);
        }
    }

    std::shared_ptr<gm::Material> resolvedMaterial;
    std::string resolvedMaterialGuid;
    const std::string originalMaterialGuid = component->MaterialGuid();
    if (!originalMaterialGuid.empty()) {
        resolvedMaterial = m_game.m_resources.GetMaterial(originalMaterialGuid);
        if (resolvedMaterial) {
            resolvedMaterialGuid = originalMaterialGuid;
            component->SetMaterial(resolvedMaterial, resolvedMaterialGuid);
        } else {
            component->SetMaterial(nullptr);
            ReportSceneIssue(
                m_game,
                fmt::format("Skinned mesh '{}' missing material '{}'", gameObject->GetName(), originalMaterialGuid),
                true);
        }
    }

    auto resolveShaderByGuid = [&](const std::string& guid,
                                   bool silent) -> gm::Shader* {
        if (guid.empty()) {
            return nullptr;
        }
        gm::Shader* shader = m_game.m_resources.GetShader(guid);
        if (!shader && !silent) {
            ReportSceneIssue(
                m_game,
                fmt::format("Skinned mesh '{}' references missing shader '{}'",
                            gameObject->GetName(),
                            guid),
                true);
        }
        if (shader) {
            shader->Use();
            shader->SetInt("uTex", 0);
            component->SetShader(shader, guid);
        }
        return shader;
    };

    std::string resolvedShaderGuid;
    gm::Shader* resolvedShader = nullptr;
    bool shaderGuidFailed = false;
    bool materialShaderFailed = false;

    const std::string requestedShaderGuid = component->ShaderGuid();
    if (!requestedShaderGuid.empty()) {
        resolvedShader = resolveShaderByGuid(requestedShaderGuid, false);
        if (resolvedShader) {
            resolvedShaderGuid = requestedShaderGuid;
        } else {
            shaderGuidFailed = true;
        }
    } else {
        component->SetShader(nullptr, std::string());
    }

    if (!resolvedShader && resolvedMaterial && !resolvedMaterialGuid.empty()) {
        if (auto shaderOverride = m_game.m_resources.GetMaterialShaderOverride(resolvedMaterialGuid)) {
            resolvedShader = resolveShaderByGuid(*shaderOverride, true);
            if (resolvedShader) {
                resolvedShaderGuid = *shaderOverride;
            } else {
                materialShaderFailed = true;
            }
        } else {
            materialShaderFailed = true;
        }
    } else if (!resolvedShader && resolvedMaterial) {
        materialShaderFailed = true;
    }

    if (!resolvedShader) {
        bool allowFallback = shaderGuidFailed || requestedShaderGuid.empty();
        if (resolvedMaterial) {
            allowFallback = allowFallback && materialShaderFailed;
        }
        if (!resolvedMaterial) {
            allowFallback = true;
        }
        if (allowFallback) {
            resolvedShader = m_game.m_resources.GetDefaultShader();
            resolvedShaderGuid = m_game.m_resources.GetShaderGuid();
            if (resolvedShader) {
                component->SetShader(resolvedShader, resolvedShaderGuid);
                resolvedShader->Use();
                resolvedShader->SetInt("uTex", 0);
                ReportSceneIssue(
                    m_game,
                    fmt::format("Skinned mesh '{}' using default shader '{}' due to missing overrides",
                                gameObject->GetName(),
                                resolvedShaderGuid.empty() ? "<unset>" : resolvedShaderGuid),
                    !resolvedMaterial);
            }
        }
    }

    if (!resolvedShader) {
        component->SetShader(nullptr, std::string());
    }

    auto resolveTexture = [&](const std::string& guid) -> std::shared_ptr<gm::Texture> {
        if (guid.empty()) {
            return nullptr;
        }
        auto textureShared = m_game.m_resources.GetTextureShared(guid);
        if (!textureShared) {
            textureShared = m_game.m_resources.EnsureTextureAvailable(guid);
        }
        return textureShared;
    };

    std::string resolvedTextureGuid = component->TextureGuid();
    if (!resolvedTextureGuid.empty()) {
        auto textureShared = resolveTexture(resolvedTextureGuid);
        if (textureShared) {
            component->SetTexture(textureShared.get(), resolvedTextureGuid);
        } else {
            component->SetTexture(static_cast<gm::Texture*>(nullptr), std::string());
            ReportSceneIssue(
                m_game,
                fmt::format("Skinned mesh '{}' missing texture '{}'",
                            gameObject->GetName(),
                            resolvedTextureGuid),
                true);
            resolvedTextureGuid.clear();
        }
    } else {
        component->SetTexture(static_cast<gm::Texture*>(nullptr), std::string());
    }

    if (resolvedTextureGuid.empty() && !resolvedMaterial) {
        if (auto* defaultTexture = m_game.m_resources.GetDefaultTexture()) {
            const std::string& defaultTextureGuid = m_game.m_resources.GetTextureGuid();
            component->SetTexture(defaultTexture, defaultTextureGuid);
            resolvedTextureGuid = defaultTextureGuid;
            ReportSceneIssue(
                m_game,
                fmt::format("Skinned mesh '{}' missing texture data; using default texture '{}'",
                            gameObject->GetName(),
                            defaultTextureGuid.empty() ? "<unset>" : defaultTextureGuid),
                false);
        }
    }

    RegisterSkinnedMeshBinding(component,
                               gameObject,
                               meshGuid,
                               component->ShaderGuid(),
                               resolvedMaterialGuid,
                               resolvedTextureGuid);
}

void SceneResourceController::ResolveAnimatorComponent(const std::shared_ptr<gm::GameObject>& gameObject,
                                                       gm::scene::AnimatorComponent* component) {
    if (!component || !gameObject) {
        return;
    }

    const std::string skeletonGuid = component->SkeletonGuid();
    if (!skeletonGuid.empty()) {
        if (auto skeletonPath = m_game.m_resources.GetSkeletonPath(skeletonGuid)) {
            try {
                gm::ResourceManager::SkeletonDescriptor desc{skeletonGuid, *skeletonPath};
                auto handle = gm::ResourceManager::LoadSkeleton(desc);
                component->SetSkeleton(std::move(handle));
            } catch (const std::exception& ex) {
                ReportSceneIssue(
                    m_game,
                    fmt::format("Failed to load skeleton '{}' for '{}': {}", skeletonGuid, gameObject->GetName(), ex.what()),
                    true);
            }
        } else {
            ReportSceneIssue(
                m_game,
                fmt::format("Animator '{}' references unknown skeleton '{}'", gameObject->GetName(), skeletonGuid),
                true);
        }
    }

    const auto layers = component->GetLayerSnapshots();
    for (const auto& layer : layers) {
        if (layer.clipGuid.empty()) {
            continue;
        }

        auto clipPath = m_game.m_resources.GetAnimationClipPath(layer.clipGuid);
        if (!clipPath) {
            ReportSceneIssue(
                m_game,
                fmt::format("Animator '{}' references unknown animation '{}'", gameObject->GetName(), layer.clipGuid),
                true);
            continue;
        }

        try {
            gm::ResourceManager::AnimationClipDescriptor desc{layer.clipGuid, *clipPath};
            auto handle = gm::ResourceManager::LoadAnimationClip(desc);
            component->SetClip(layer.slot, std::move(handle));
            component->SetWeight(layer.slot, layer.weight);
            if (layer.playing) {
                component->Play(layer.slot, layer.loop);
            } else {
                component->Stop(layer.slot);
            }
        } catch (const std::exception& ex) {
            ReportSceneIssue(
                m_game,
                fmt::format("Failed to load animation '{}' for '{}' slot '{}': {}",
                            layer.clipGuid,
                            gameObject->GetName(),
                            layer.slot,
                            ex.what()),
                true);
        }
    }
}

