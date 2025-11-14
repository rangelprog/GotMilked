#if GM_DEBUG_TOOLS

#include "../DebugMenu.hpp"
#include "../GameResources.hpp"

#include "gm/scene/Scene.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/StaticMeshComponent.hpp"
#include "gm/scene/SkinnedMeshComponent.hpp"
#include "gm/scene/AnimatorComponent.hpp"
#include "gm/scene/LightComponent.hpp"
#include "gm/core/Logger.hpp"

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cctype>
#include <cstring>

namespace {
constexpr const char* kSceneHierarchyPayload = "GM_SCENE_GAMEOBJECT";
}

namespace gm::debug {

void DebugMenu::RenderSceneExplorerWindow() {
    if (ShouldDelaySceneUI()) {
        return;
    }

    if (!m_showSceneExplorer) {
        return;
    }

    if (!ImGui::Begin("Scene Explorer", &m_showSceneExplorer)) {
        ImGui::End();
        return;
    }

    RenderSceneHierarchy();
    ImGui::Separator();
    RenderInspector();

    ImGui::End();
}

void DebugMenu::RenderSceneHierarchy() {
    auto scene = m_scene.lock();
    if (!scene) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No scene available");
        return;
    }

    const std::uint64_t sceneVersion = scene->CurrentReloadVersion();
    if (sceneVersion != m_lastSeenSceneVersion) {
        m_lastSeenSceneVersion = sceneVersion;
    }

    static char searchFilter[256] = {0};
    ImGui::InputText("Search", searchFilter, sizeof(searchFilter));
    ImGui::Separator();

    std::string filterStr(searchFilter);
    std::string lowerFilter = filterStr;
    std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(),
                   [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });

    auto allObjects = scene->GetAllGameObjects();

    if (lowerFilter.empty()) {
        auto roots = scene->GetRootGameObjects();
        RenderSceneHierarchyTree(roots, lowerFilter);
        RenderSceneHierarchyRootDropTarget();
        if (roots.empty()) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No GameObjects in scene");
        }
    } else {
        RenderSceneHierarchyFiltered(allObjects, lowerFilter);
        RenderSceneHierarchyRootDropTarget();
    }
}

void DebugMenu::RenderSceneHierarchyTree(const std::vector<std::shared_ptr<gm::GameObject>>& roots, const std::string& filter) {
    (void)filter;
    for (const auto& root : roots) {
        RenderSceneHierarchyNode(root, filter);
    }
}

void DebugMenu::RenderSceneHierarchyNode(const std::shared_ptr<gm::GameObject>& gameObject, const std::string& filter) {
    (void)filter;
    if (!gameObject || gameObject->IsDestroyed()) {
        return;
    }

    std::string name = gameObject->GetName();
    if (name.empty()) {
        name = "Unnamed GameObject";
    }
    std::string displayName = name;
    if (!gameObject->IsActive()) {
        displayName += " [Inactive]";
    }

    auto children = gameObject->GetChildren();
    children.erase(std::remove_if(children.begin(), children.end(),
                                  [](const std::shared_ptr<gm::GameObject>& child) {
                                      return !child || child->IsDestroyed();
                                  }),
                   children.end());

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    auto selected = m_selectedGameObject.lock();
    if (selected && selected == gameObject) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    ImGui::PushID(gameObject.get());
    bool open = ImGui::TreeNodeEx(displayName.c_str(), flags);
    if (ImGui::IsItemClicked()) {
        m_selectedGameObject = gameObject;
        EnsureSelectionWindowsVisible();
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        EnsureSelectionWindowsVisible();
        FocusCameraOnGameObject(gameObject);
    }

    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        gm::GameObject* raw = gameObject.get();
        ImGui::SetDragDropPayload(kSceneHierarchyPayload, &raw, sizeof(gm::GameObject*));
        ImGui::TextUnformatted(displayName.c_str());
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kSceneHierarchyPayload)) {
            if (auto dragged = ResolvePayloadGameObject(payload)) {
                if (dragged != gameObject) {
                    if (auto scenePtr = m_scene.lock()) {
                        scenePtr->SetParent(dragged, gameObject);
                    }
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    bool deletedFromContext = false;
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Unparent", nullptr, false, gameObject->HasParent())) {
            if (auto scenePtr = m_scene.lock()) {
                scenePtr->SetParent(gameObject, std::shared_ptr<gm::GameObject>());
            }
        }
        if (ImGui::MenuItem("Focus Camera")) {
            FocusCameraOnGameObject(gameObject);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Delete")) {
            DeleteGameObject(gameObject);
            deletedFromContext = true;
        }
        ImGui::EndPopup();
    }

    if (deletedFromContext) {
        ImGui::PopID();
        return;
    }

    if (open && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
        for (const auto& child : children) {
            RenderSceneHierarchyNode(child, filter);
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

void DebugMenu::RenderSceneHierarchyFiltered(const std::vector<std::shared_ptr<gm::GameObject>>& objects, const std::string& filter) {
    auto scene = m_scene.lock();
    auto selected = m_selectedGameObject.lock();
    int visibleCount = 0;

    for (const auto& gameObject : objects) {
        if (!gameObject || gameObject->IsDestroyed()) {
            continue;
        }

        std::string name = gameObject->GetName();
        if (name.empty()) {
            name = "Unnamed GameObject";
        }

        std::string lowerName = name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                       [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
        if (lowerName.find(filter) == std::string::npos) {
            continue;
        }

        std::string displayName = name;
        if (!gameObject->IsActive()) {
            displayName += " [Inactive]";
        }

        bool isSelected = (selected && selected == gameObject);

        ImGui::PushID(gameObject.get());
        if (ImGui::Selectable(displayName.c_str(), isSelected)) {
            m_selectedGameObject = gameObject;
            EnsureSelectionWindowsVisible();
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            EnsureSelectionWindowsVisible();
            FocusCameraOnGameObject(gameObject);
        }

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            gm::GameObject* raw = gameObject.get();
            ImGui::SetDragDropPayload(kSceneHierarchyPayload, &raw, sizeof(gm::GameObject*));
            ImGui::TextUnformatted(displayName.c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kSceneHierarchyPayload)) {
                if (auto dragged = ResolvePayloadGameObject(payload)) {
                    if (dragged != gameObject && scene) {
                        scene->SetParent(dragged, gameObject);
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        bool deletedFromContext = false;
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Unparent", nullptr, false, gameObject->HasParent())) {
                if (scene) {
                    scene->SetParent(gameObject, std::shared_ptr<gm::GameObject>());
                }
            }
            if (ImGui::MenuItem("Focus Camera")) {
                FocusCameraOnGameObject(gameObject);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete")) {
                DeleteGameObject(gameObject);
                deletedFromContext = true;
            }
            ImGui::EndPopup();
        }

        if (deletedFromContext) {
            ImGui::PopID();
            continue;
        }

        ImGui::PopID();
        ++visibleCount;
    }

    if (visibleCount == 0) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No GameObjects match filter");
    }
}

void DebugMenu::RenderSceneHierarchyRootDropTarget() {
    ImVec2 dropSize = ImGui::GetContentRegionAvail();
    dropSize.y = std::max(dropSize.y, 24.0f);
    ImGui::InvisibleButton("##SceneHierarchyRootDrop", ImVec2(dropSize.x, std::min(dropSize.y, 32.0f)));
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kSceneHierarchyPayload)) {
            if (auto dragged = ResolvePayloadGameObject(payload)) {
                if (auto scenePtr = m_scene.lock()) {
                    scenePtr->SetParent(dragged, std::shared_ptr<gm::GameObject>());
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
        ImGui::SetTooltip("Drop here to unparent");
    }
}

std::shared_ptr<gm::GameObject> DebugMenu::ResolvePayloadGameObject(const ImGuiPayload* payload) {
    if (!payload || payload->DataSize != sizeof(gm::GameObject*)) {
        return nullptr;
    }
    auto scene = m_scene.lock();
    if (!scene) {
        return nullptr;
    }
    auto rawPtr = *static_cast<gm::GameObject* const*>(payload->Data);
    if (!rawPtr) {
        return nullptr;
    }
    return scene->FindGameObjectByPointer(rawPtr);
}

void DebugMenu::DeleteGameObject(const std::shared_ptr<gm::GameObject>& gameObject) {
    if (!gameObject) {
        return;
    }

    auto scene = m_scene.lock();
    if (!scene) {
        return;
    }

    scene->DestroyGameObject(gameObject);

    auto selected = m_selectedGameObject.lock();
    if (!selected || selected == gameObject || selected->IsDestroyed()) {
        ClearSelection();
    }
}

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS

