#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

namespace gm {
class GameObject;
namespace scene {
class StaticMeshComponent;
}
} // namespace gm

class Game;

class SceneResourceController {
public:
    explicit SceneResourceController(Game& game);

    void ApplyResourcesToScene();
    void ApplyResourcesToStaticMeshComponents();
#if GM_DEBUG_TOOLS
    void ApplyResourcesToTerrain();
#endif

    void RefreshShaders(const std::vector<std::string>& guids);
    void RefreshMeshes(const std::vector<std::string>& guids);
    void RefreshMaterials(const std::vector<std::string>& guids);

private:
    Game& m_game;

    struct StaticMeshBinding {
        std::weak_ptr<gm::GameObject> owner;
        std::string meshGuid;
        std::string shaderGuid;
        std::string materialGuid;
    };

    std::unordered_map<gm::scene::StaticMeshComponent*, StaticMeshBinding> m_staticMeshBindings;
    std::unordered_map<std::string, std::unordered_set<gm::scene::StaticMeshComponent*>> m_meshDependents;
    std::unordered_map<std::string, std::unordered_set<gm::scene::StaticMeshComponent*>> m_shaderDependents;
    std::unordered_map<std::string, std::unordered_set<gm::scene::StaticMeshComponent*>> m_materialDependents;

    void ClearStaticMeshDependencies();
    void RemoveStaticMeshBinding(gm::scene::StaticMeshComponent* component);
    void RegisterStaticMeshBinding(gm::scene::StaticMeshComponent* component,
                                   const std::shared_ptr<gm::GameObject>& owner,
                                   const std::string& meshGuid,
                                   const std::string& shaderGuid,
                                   const std::string& materialGuid);
    void ResolveStaticMeshComponent(const std::shared_ptr<gm::GameObject>& gameObject,
                                    gm::scene::StaticMeshComponent* meshComp);
    void ResolveStaticMeshComponentBinding(gm::scene::StaticMeshComponent* component);
    static void EraseDependent(std::unordered_map<std::string, std::unordered_set<gm::scene::StaticMeshComponent*>>& map,
                               const std::string& guid,
                               gm::scene::StaticMeshComponent* component);
};

