#if GM_DEBUG_TOOLS

#include "DebugMenu.hpp"
#include "EditableTerrainComponent.hpp"
#include "GameConstants.hpp"
#include "GameResources.hpp"
#include "gm/scene/PrefabLibrary.hpp"

#include "gm/scene/Scene.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/StaticMeshComponent.hpp"
#include "gm/scene/SkinnedMeshComponent.hpp"
#include "gm/scene/AnimatorComponent.hpp"
#include "gm/scene/LightComponent.hpp"
#include "gm/physics/RigidBodyComponent.hpp"
#include "gm/animation/AnimationClip.hpp"
#include "gm/animation/AnimationPoseEvaluator.hpp"
#include "gm/animation/Skeleton.hpp"
#include "gm/core/Logger.hpp"
#include "gm/assets/AssetCatalog.hpp"
#include "gm/utils/ResourceManager.hpp"

#include <cstdint>

#include <imgui.h>
#include <imgui_internal.h>
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <ImGuizmo.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <unordered_map>
#include <vector>
#include <functional>
#include <cctype>
#include <fmt/format.h>
#include <limits>
#include <nlohmann/json.hpp>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <windows.h>
#endif

namespace {
ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) {
    return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y);
}

ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) {
    return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
}

}

namespace gm::debug {

void DebugMenu::RenderDockspace() {
#if defined(IMGUI_HAS_DOCK)
    ImGuiIO& io = ImGui::GetIO();
    if ((io.ConfigFlags & ImGuiConfigFlags_DockingEnable) == 0) {
        return;
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking |
                                   ImGuiWindowFlags_NoTitleBar |
                                   ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                                   ImGuiWindowFlags_NoNavFocus |
                                   ImGuiWindowFlags_NoBackground;

    ImGui::Begin("GM_DebugDockspaceHost", nullptr, windowFlags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspaceId = ImGui::GetID("GM_DebugDockspace");
    ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode;
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockFlags);

    if (m_pendingDockRestore && !m_cachedDockspaceLayout.empty()) {
        ImGui::LoadIniSettingsFromMemory(m_cachedDockspaceLayout.c_str());
        m_pendingDockRestore = false;
    }

    static bool dockInitialized = false;
    if (m_resetDockLayout) {
        dockInitialized = false;
        m_resetDockLayout = false;
    }

    if (!dockInitialized) {
        dockInitialized = true;
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->Size);

        ImGuiID dockLeft = ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.28f, nullptr, &dockspaceId);
        ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Right, 0.30f, nullptr, &dockspaceId);
        ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Down, 0.30f, nullptr, &dockspaceId);

        ImGui::DockBuilderDockWindow("Scene Explorer", dockLeft);
        ImGui::DockBuilderDockWindow("Prefab Browser", dockBottom);
        ImGui::DockBuilderDockWindow("Scene Info", dockRight);
        ImGui::DockBuilderFinish(dockspaceId);
    }

    ImGui::End();
#else
    (void)m_resetDockLayout;
#endif
}

void DebugMenu::EnsureSelectionWindowsVisible() {
    if (!m_showSceneExplorer) {
        m_showSceneExplorer = true;
    }
}

void DebugMenu::FocusCameraOnGameObject(const std::shared_ptr<gm::GameObject>& gameObject) {
    if (!gameObject) {
        return;
    }

    if (!m_callbacks.setCamera) {
        return;
    }

    auto transform = gameObject->GetTransform();
    if (!transform) {
        return;
    }

    const glm::vec3 targetPosition = transform->GetPosition();
    const glm::vec3 defaultOffsetDirection = glm::normalize(glm::vec3(-0.55f, 0.45f, -0.6f));
    const float focusDistance = 7.5f;

    glm::vec3 desiredPosition = targetPosition + defaultOffsetDirection * focusDistance;
    desiredPosition.y = std::max(desiredPosition.y, targetPosition.y + 2.0f);

    glm::vec3 forwardVector = targetPosition - desiredPosition;
    const float forwardLength = glm::length(forwardVector);
    if (forwardLength < 1e-6f) {
        forwardVector = glm::vec3(0.0f, 0.0f, -1.0f);
    } else {
        forwardVector /= forwardLength;
    }

    float cameraFov = m_callbacks.getCameraFov ? m_callbacks.getCameraFov() : gotmilked::GameConstants::Camera::DefaultFovDegrees;
    m_callbacks.setCamera(desiredPosition, forwardVector, cameraFov);
}

void DebugMenu::RenderGameObjectOverlay() {
    if (ShouldDelaySceneUI()) {
        return;
    }

    auto scene = m_scene.lock();
    if (!scene) {
        return;
    }

    if (!m_callbacks.getViewMatrix || !m_callbacks.getProjectionMatrix || !m_callbacks.getViewportSize) {
        return;
    }

    glm::mat4 view = m_callbacks.getViewMatrix();
    glm::mat4 proj = m_callbacks.getProjectionMatrix();
    int viewportWidth = 0;
    int viewportHeight = 0;
    m_callbacks.getViewportSize(viewportWidth, viewportHeight);

    if (viewportWidth <= 0 || viewportHeight <= 0) {
        return;
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
#if IMGUI_VERSION_NUM >= 18200 && defined(IMGUI_HAS_DOCK)
    ImGui::SetNextWindowViewport(viewport->ID);
#endif
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::SetNextWindowBgAlpha(0.0f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoNav |
                             ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoInputs;

    if (!ImGui::Begin("GM_GameObjectOverlay", nullptr, flags)) {
        ImGui::End();
        ImGui::PopStyleVar(3);
        return;
    }

    // Grid overlay has been disabled for now to avoid appearing on top of all objects.

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const glm::mat4 viewProj = proj * view;

    auto selected = m_selectedGameObject.lock();

    const float circleRadius = 6.0f;
    const float hoverRadius = circleRadius * 1.8f;
    const ImU32 defaultColor = IM_COL32(255, 255, 255, 255);
    const ImU32 selectedColor = IM_COL32(255, 220, 0, 255);

    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 mousePos = io.MousePos;

    const auto projectToScreen = [&](const glm::vec3& worldPos, ImVec2& outScreenPos) -> bool {
        glm::vec4 clipPos = viewProj * glm::vec4(worldPos, 1.0f);
        if (std::abs(clipPos.w) < 1e-6f) {
            return false;
        }

        glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
        if (ndc.z < -1.0f || ndc.z > 1.0f ||
            ndc.x < -1.0f || ndc.x > 1.0f ||
            ndc.y < -1.0f || ndc.y > 1.0f) {
            return false;
        }

        float screenX = (ndc.x + gotmilked::GameConstants::Rendering::NdcOffset) *
                        gotmilked::GameConstants::Rendering::NdcToScreenScale *
                        static_cast<float>(viewportWidth);
        float screenY = (gotmilked::GameConstants::Rendering::NdcOffset - ndc.y) *
                        gotmilked::GameConstants::Rendering::NdcToScreenScale *
                        static_cast<float>(viewportHeight);

        outScreenPos = ImVec2(viewport->Pos.x + screenX, viewport->Pos.y + screenY);
        return true;
    };

    for (const auto& gameObject : scene->GetAllGameObjects()) {
        if (!gameObject || !gameObject->IsActive() || gameObject->IsDestroyed()) {
            continue;
        }

        auto transform = gameObject->GetTransform();
        if (!transform) {
            continue;
        }

        ImVec2 screenPos{};
        if (!projectToScreen(transform->GetPosition(), screenPos)) {
            continue;
        }

        bool isHovered = (io.MousePos.x >= viewport->Pos.x && io.MousePos.x <= viewport->Pos.x + viewport->Size.x &&
                          io.MousePos.y >= viewport->Pos.y && io.MousePos.y <= viewport->Pos.y + viewport->Size.y &&
                          ImLengthSqr(mousePos - screenPos) <= (hoverRadius * hoverRadius));

        if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_selectedGameObject = gameObject;
            selected = gameObject;
            EnsureSelectionWindowsVisible();
        }

        if (isHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            EnsureSelectionWindowsVisible();
            FocusCameraOnGameObject(gameObject);
        }

        bool isSelected = (selected && selected == gameObject);

        ImU32 color = isSelected ? selectedColor : defaultColor;
        if (isHovered) {
            color = IM_COL32(180, 255, 255, 255);
        }

        drawList->AddCircleFilled(screenPos, circleRadius, color, 16);

        const std::string& name = gameObject->GetName();
        if (!name.empty()) {
            ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
            ImVec2 textPos(screenPos.x - textSize.x * 0.5f, screenPos.y - circleRadius - textSize.y - 4.0f);
            drawList->AddRectFilled(ImVec2(textPos.x - 4.0f, textPos.y - 2.0f),
                                    ImVec2(textPos.x + textSize.x + 4.0f, textPos.y + textSize.y + 2.0f),
                                    IM_COL32(0, 0, 0, 180), 3.0f);
            drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), name.c_str());
        }
    }

    auto drawBonesForObject = [&](const std::shared_ptr<gm::GameObject>& target) {
        if (!target) {
            return;
        }

        auto animator = target->GetComponent<gm::scene::AnimatorComponent>();
        auto transform = target->GetTransform();
        if (!animator || !transform) {
            return;
        }

        std::vector<glm::mat4> boneMatrices;
        if (!animator->GetBoneModelMatrices(boneMatrices)) {
            return;
        }

        auto skeleton = animator->GetSkeletonAsset();
        if (!skeleton || boneMatrices.size() != skeleton->bones.size()) {
            return;
        }

        const glm::mat4 modelMatrix = transform->GetMatrix();
        std::vector<ImVec2> screenPositions(boneMatrices.size());
        std::vector<uint8_t> visible(boneMatrices.size(), 0);

        for (std::size_t i = 0; i < boneMatrices.size(); ++i) {
            glm::mat4 worldMatrix = modelMatrix * boneMatrices[i];
            glm::vec3 worldPos(worldMatrix[3]);
            visible[i] = projectToScreen(worldPos, screenPositions[i]) ? 1 : 0;
        }

        const ImU32 lineColor = IM_COL32(0, 210, 255, 220);
        for (std::size_t i = 0; i < boneMatrices.size(); ++i) {
            if (!visible[i]) {
                continue;
            }

            const auto& bone = skeleton->bones[i];
            if (bone.parentIndex >= 0) {
                const std::size_t parentIndex = static_cast<std::size_t>(bone.parentIndex);
                if (parentIndex < visible.size() && visible[parentIndex]) {
                    drawList->AddLine(screenPositions[parentIndex], screenPositions[i], lineColor, m_boneOverlayLineThickness);
                }
            }

            drawList->AddCircleFilled(screenPositions[i], m_boneOverlayNodeRadius, lineColor, 10);
            if (m_showBoneNames) {
                const std::string& boneName = bone.name.empty() ? std::to_string(i) : bone.name;
                drawList->AddText(ImVec2(screenPositions[i].x + 4.0f, screenPositions[i].y),
                                  IM_COL32(240, 240, 240, 230), boneName.c_str());
            }
        }
    };

    if (m_enableBoneOverlay) {
        if (m_boneOverlayAllObjects) {
            for (const auto& object : scene->GetAllGameObjects()) {
                drawBonesForObject(object);
            }
        } else if (selected) {
            drawBonesForObject(selected);
        }
    }

    if (m_showAnimationDebugOverlay && selected) {
        auto animator = selected->GetComponent<gm::scene::AnimatorComponent>();
        if (animator) {
            auto snapshots = animator->GetLayerSnapshots();
            std::string panelText = fmt::format("Animator: {}\nSkeleton: {}\n",
                                                selected->GetName(),
                                                animator->SkeletonGuid().empty() ? "<none>" : animator->SkeletonGuid());
            if (snapshots.empty()) {
                panelText += "No layers\n";
            } else {
                for (const auto& layer : snapshots) {
                    panelText += fmt::format("{} | clip={} | w={:.2f} | t={:.2f}s | {}\n",
                                             layer.slot,
                                             layer.clipGuid.empty() ? "<none>" : layer.clipGuid,
                                             layer.weight,
                                             layer.timeSeconds,
                                             layer.playing ? "Playing" : "Paused");
                }
            }

            ImVec2 panelPos(viewport->Pos.x + 20.0f, viewport->Pos.y + 20.0f);
            ImVec2 textSize = ImGui::CalcTextSize(panelText.c_str());
            ImVec2 padding(8.0f, 6.0f);
            drawList->AddRectFilled(panelPos - padding,
                                    panelPos + textSize + padding,
                                    IM_COL32(0, 0, 0, 170), 6.0f);
            drawList->AddRect(panelPos - padding,
                              panelPos + textSize + padding,
                              IM_COL32(0, 200, 255, 220), 6.0f);
            drawList->AddText(panelPos, IM_COL32(230, 230, 230, 255), panelText.c_str());
        }
    }

    ImGui::End();
    ImGui::PopStyleVar(3);
}

void DebugMenu::RenderInspector() {
    auto scene = m_scene.lock();
    auto selected = m_selectedGameObject.lock();
    if (!selected) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No GameObject selected");
        ImGui::Text("Select a GameObject from the Scene Hierarchy");
        return;
    }

    char nameBuffer[256];
    const std::string& name = selected->GetName();
    std::size_t copyLen = std::min(name.length(), sizeof(nameBuffer) - 1);
    std::memcpy(nameBuffer, name.c_str(), copyLen);
    nameBuffer[copyLen] = '\0';
    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
        selected->SetName(nameBuffer);
    }

    bool isActive = selected->IsActive();
    if (ImGui::Checkbox("Active", &isActive)) {
        selected->SetActive(isActive);
        if (scene) {
            scene->MarkActiveListsDirty();
        }
    }

    auto parent = selected->GetParent();
    if (parent) {
        ImGui::Text("Parent: %s", parent->GetName().c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Focus Parent")) {
            FocusCameraOnGameObject(parent);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Unparent")) {
            if (scene) {
                scene->SetParent(selected, std::shared_ptr<gm::GameObject>());
            }
        }
    } else {
        ImGui::TextUnformatted("Parent: <None>");
    }

    ImGui::Separator();
    if (ImGui::Button("Delete GameObject")) {
        DeleteGameObject(selected);
        return;
    }

    ImGui::Separator();

    auto transform = selected->GetTransform();
    if (transform) {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            glm::vec3 pos = transform->GetPosition();
            if (ImGui::DragFloat3("Position", glm::value_ptr(pos), 0.1f)) {
                transform->SetPosition(pos);
                if (scene) {
                    scene->MarkActiveListsDirty();
                }
            }

            glm::vec3 rot = transform->GetRotation();
            if (ImGui::DragFloat3("Rotation", glm::value_ptr(rot), 1.0f)) {
                transform->SetRotation(rot);
                if (scene) {
                    scene->MarkActiveListsDirty();
                }
            }

            glm::vec3 scale = transform->GetScale();
            if (ImGui::DragFloat3("Scale", glm::value_ptr(scale), 0.01f)) {
                transform->SetScale(scale);
                if (scene) {
                    scene->MarkActiveListsDirty();
                }
            }

            if (ImGui::TreeNodeEx("Local Transform", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth)) {
                glm::vec3 localPos = transform->GetLocalPosition();
                if (ImGui::DragFloat3("Local Position", glm::value_ptr(localPos), 0.1f)) {
                    transform->SetLocalPosition(localPos);
                }

                glm::vec3 localRot = transform->GetLocalRotation();
                if (ImGui::DragFloat3("Local Rotation", glm::value_ptr(localRot), 1.0f)) {
                    transform->SetLocalRotation(localRot);
                }

                glm::vec3 localScale = transform->GetLocalScale();
                if (ImGui::DragFloat3("Local Scale", glm::value_ptr(localScale), 0.01f)) {
                    transform->SetLocalScale(localScale);
                }
                ImGui::TreePop();
            }

            ImGui::Separator();
            const char* opItems[] = { "Translate", "Rotate", "Scale" };
            ImGui::Combo("Gizmo Operation", &m_gizmoOperation, opItems, IM_ARRAYSIZE(opItems));
            const char* modeItems[] = { "World", "Local" };
            ImGui::Combo("Gizmo Mode", &m_gizmoMode, modeItems, IM_ARRAYSIZE(modeItems));
            ImGui::TextUnformatted("Hotkeys: W (Translate), E (Rotate), R (Scale)");
        }
    }

    auto buildSortedGuidList = [](const auto& map) {
        std::vector<std::string> guids;
        guids.reserve(map.size());
        for (const auto& entry : map) {
            guids.push_back(entry.first);
        }
        std::sort(guids.begin(), guids.end());
        return guids;
    };

    auto drawGuidCombo = [](const char* label,
                            const std::vector<std::string>& guids,
                            const std::string& currentGuid,
                            const std::function<void(const std::string&)>& onSelect) {
        const char* preview = currentGuid.empty() ? "None" : currentGuid.c_str();
        if (ImGui::BeginCombo(label, preview)) {
            for (const auto& guid : guids) {
                bool selected = (guid == currentGuid);
                if (ImGui::Selectable(guid.c_str(), selected)) {
                    onSelect(guid);
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    };

    auto components = selected->GetComponents();
    int componentIndex = 0;
    for (const auto& component : components) {
        if (!component || component == transform) {
            continue;
        }

        std::string compName = component->GetName();
        if (compName.empty()) {
            gm::core::Logger::Error("[DebugMenu] Component on '{}' missing name (typeid: {})",
                selected->GetName().c_str(),
                typeid(*component).name());
            compName = "Component";
        }

        ImGui::PushID(component.get());
        if (ImGui::CollapsingHeader(compName.c_str())) {
            if (auto meshComp = std::dynamic_pointer_cast<gm::scene::StaticMeshComponent>(component)) {
                ImGui::Text("Mesh GUID: %s", meshComp->GetMeshGuid().empty() ? "None" : meshComp->GetMeshGuid().c_str());
                ImGui::Text("Shader GUID: %s", meshComp->GetShaderGuid().empty() ? "None" : meshComp->GetShaderGuid().c_str());
                ImGui::Text("Material GUID: %s", meshComp->GetMaterialGuid().empty() ? "None" : meshComp->GetMaterialGuid().c_str());
                ImGui::Text("Has Mesh: %s", meshComp->GetMesh() ? "Yes" : "No");
                ImGui::Text("Has Shader: %s", meshComp->GetShader() ? "Yes" : "No");
                ImGui::Text("Has Material: %s", meshComp->GetMaterial() ? "Yes" : "No");

                if (m_gameResources) {
                    ImGui::Separator();
                    ImGui::TextUnformatted("Assign Resources");

                    const auto meshGuids = buildSortedGuidList(m_gameResources->GetMeshMap());
                    const auto shaderGuids = buildSortedGuidList(m_gameResources->GetShaderMap());
                    const auto materialGuids = buildSortedGuidList(m_gameResources->GetMaterialMap());

                    if (!meshGuids.empty()) {
                        drawGuidCombo("Mesh Asset", meshGuids, meshComp->GetMeshGuid(), [this, &meshComp](const std::string& guid) {
                            if (auto* mesh = m_gameResources->GetMesh(guid)) {
                                meshComp->SetMesh(mesh, guid);
                            } else {
                                gm::core::Logger::Warning("[DebugMenu] Mesh '{}' not available in GameResources", guid);
                            }
                        });
                        if (ImGui::Button("Clear Mesh")) {
                            meshComp->SetMesh(nullptr, "");
                        }
                        if (auto path = m_gameResources->GetMeshSource(meshComp->GetMeshGuid())) {
                            ImGui::TextWrapped("Path: %s", path->c_str());
                        }
                    }

                    if (!shaderGuids.empty()) {
                        drawGuidCombo("Shader Asset", shaderGuids, meshComp->GetShaderGuid(), [this, &meshComp](const std::string& guid) {
                            if (auto* shader = m_gameResources->GetShader(guid)) {
                                meshComp->SetShader(shader, guid);
                                shader->Use();
                                shader->SetInt("uTex", 0);
                            } else {
                                gm::core::Logger::Warning("[DebugMenu] Shader '{}' not available in GameResources", guid);
                            }
                        });
                        if (ImGui::Button("Clear Shader")) {
                            meshComp->SetShader(nullptr, "");
                        }
                        if (auto shaderSource = m_gameResources->GetShaderSource(meshComp->GetShaderGuid())) {
                            ImGui::TextWrapped("Vert: %s", shaderSource->vertexPath.c_str());
                            ImGui::TextWrapped("Frag: %s", shaderSource->fragmentPath.c_str());
                        }
                    }

                    if (!materialGuids.empty()) {
                        drawGuidCombo("Material Asset", materialGuids, meshComp->GetMaterialGuid(), [this, &meshComp](const std::string& guid) {
                            auto material = m_gameResources->GetMaterial(guid);
                            if (material) {
                                meshComp->SetMaterial(std::move(material), guid);
                            } else {
                                gm::core::Logger::Warning("[DebugMenu] Material '{}' not available in GameResources", guid);
                            }
                        });
                        if (ImGui::Button("Clear Material")) {
                            meshComp->SetMaterial(nullptr, "");
                        }
                    }
                }
            } else if (auto skinned = std::dynamic_pointer_cast<gm::scene::SkinnedMeshComponent>(component)) {
                ImGui::Text("Skinned Mesh GUID: %s", skinned->MeshGuid().empty() ? "None" : skinned->MeshGuid().c_str());
                ImGui::Text("Shader GUID: %s", skinned->ShaderGuid().empty() ? "None" : skinned->ShaderGuid().c_str());
                ImGui::Text("Material GUID: %s", skinned->MaterialGuid().empty() ? "None" : skinned->MaterialGuid().c_str());
                ImGui::Text("Texture GUID: %s", skinned->TextureGuid().empty() ? "None" : skinned->TextureGuid().c_str());
                ImGui::Text("Has Mesh: %s", skinned->Mesh() ? "Yes" : "No");
                ImGui::Text("Has Shader: %s", skinned->GetShader() ? "Yes" : "No");
                ImGui::Text("Has Material: %s", skinned->GetMaterial() ? "Yes" : "No");
                ImGui::Text("Has Texture: %s", skinned->GetTexture() ? "Yes" : "No");

                if (ImGui::Button("Open Animation Preview##SkinnedMesh")) {
                    m_showAnimationDebugger = true;
                }
                ImGui::SameLine();
                ImGui::Checkbox("Bone Overlay##SkinnedMesh", &m_enableBoneOverlay);

                if (m_gameResources) {
                    ImGui::Separator();
                    ImGui::TextUnformatted("Assign Resources");

                    const auto meshGuids = buildSortedGuidList(m_gameResources->GetSkinnedMeshSources());
                    const auto shaderGuids = buildSortedGuidList(m_gameResources->GetShaderMap());
                    const auto materialGuids = buildSortedGuidList(m_gameResources->GetMaterialMap());
                    const auto textureGuids = buildSortedGuidList(m_gameResources->GetTextureMap());

                    if (!meshGuids.empty()) {
                        drawGuidCombo("Skinned Mesh Asset", meshGuids, skinned->MeshGuid(), [this, &skinned](const std::string& guid) {
                            if (auto path = m_gameResources->GetSkinnedMeshPath(guid)) {
                                try {
                                    gm::ResourceManager::SkinnedMeshDescriptor desc{guid, *path};
                                    auto handle = gm::ResourceManager::LoadSkinnedMesh(desc);
                                    skinned->SetMesh(std::move(handle));
                                } catch (const std::exception& ex) {
                                    gm::core::Logger::Error("[DebugMenu] Failed to load skinned mesh '{}': {}", guid, ex.what());
                                }
                            } else {
                                gm::core::Logger::Warning("[DebugMenu] Skinned mesh '{}' has no registered asset path", guid);
                            }
                        });
                        if (ImGui::Button("Clear Skinned Mesh##Skinned")) {
                            skinned->SetMesh(std::shared_ptr<gm::animation::SkinnedMeshAsset>{}, "");
                        }
                        if (auto path = m_gameResources->GetSkinnedMeshPath(skinned->MeshGuid())) {
                            ImGui::TextWrapped("Path: %s", path->c_str());
                        }
                    }

                    if (!shaderGuids.empty()) {
                        drawGuidCombo("Shader Asset##Skinned", shaderGuids, skinned->ShaderGuid(), [this, &skinned](const std::string& guid) {
                            if (auto* shader = m_gameResources->GetShader(guid)) {
                                skinned->SetShader(shader, guid);
                                shader->Use();
                                shader->SetInt("uTex", 0);
                            } else {
                                gm::core::Logger::Warning("[DebugMenu] Shader '{}' not available in GameResources", guid);
                            }
                        });
                        if (ImGui::Button("Clear Shader##Skinned")) {
                            skinned->SetShader(nullptr, "");
                        }
                    }

                    if (!materialGuids.empty()) {
                        drawGuidCombo("Material Asset##Skinned", materialGuids, skinned->MaterialGuid(), [this, &skinned](const std::string& guid) {
                            auto material = m_gameResources->GetMaterial(guid);
                            if (material) {
                                skinned->SetMaterial(std::move(material), guid);
                            } else {
                                gm::core::Logger::Warning("[DebugMenu] Material '{}' not available in GameResources", guid);
                            }
                        });
                        if (ImGui::Button("Clear Material##Skinned")) {
                            skinned->SetMaterial(nullptr, "");
                        }
                    }

                    if (!textureGuids.empty()) {
                        drawGuidCombo("Texture Asset##Skinned", textureGuids, skinned->TextureGuid(), [this, &skinned](const std::string& guid) {
                            auto texture = m_gameResources->EnsureTextureAvailable(guid);
                            if (texture) {
                                skinned->SetTexture(texture.get(), guid);
                            } else {
                                gm::core::Logger::Warning("[DebugMenu] Texture '{}' not available in GameResources", guid);
                            }
                        });
                        if (ImGui::Button("Clear Texture##Skinned")) {
                            skinned->SetTexture(static_cast<gm::Texture*>(nullptr), "");
                        }
                    }

                    if (const auto* animset = m_gameResources->GetAnimsetRecordForSkinnedMesh(skinned->MeshGuid())) {
                        ImGui::Separator();
                        ImGui::TextUnformatted("GLB Import");
                        ImGui::TextWrapped("Source: %s", animset->sourceGlb.c_str());
                        if (ImGui::Button("Re-import GLB##Skinned")) {
                            TriggerGlbReimport(skinned->MeshGuid());
                        }
                    }
                }
            } else if (auto animatorComp = std::dynamic_pointer_cast<gm::scene::AnimatorComponent>(component)) {
                const char* skeletonGuid = animatorComp->SkeletonGuid().empty() ? "None" : animatorComp->SkeletonGuid().c_str();
                ImGui::Text("Skeleton GUID: %s", skeletonGuid);
                auto skeletonAsset = animatorComp->GetSkeletonAsset();
                ImGui::Text("Bones: %zu", skeletonAsset ? skeletonAsset->bones.size() : 0);

                if (ImGui::Button("Animation Preview##AnimatorInspector")) {
                    m_showAnimationDebugger = true;
                }
                ImGui::SameLine();
                ImGui::Checkbox("Bone Overlay##AnimatorInspector", &m_enableBoneOverlay);

                EnsureAnimationAssetCache();
                if (!m_animationSkeletonAssets.empty()) {
                    const char* preview = skeletonGuid;
                    if (ImGui::BeginCombo("Assign Skeleton", preview)) {
                        for (const auto& entry : m_animationSkeletonAssets) {
                            bool selectedSkeleton = (animatorComp->SkeletonGuid() == entry.displayName);
                            if (ImGui::Selectable(entry.displayName.c_str(), selectedSkeleton)) {
                                AssignSkeletonFromAsset(*animatorComp, entry);
                            }
                            if (selectedSkeleton) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                } else {
                    ImGui::TextUnformatted("No skeleton assets detected.");
                }

                if (ImGui::Button("Refresh Animation Assets##Animator")) {
                    m_animationAssetsDirty = true;
                    EnsureAnimationAssetCache();
                }

                ImGui::Separator();
                ImGui::TextUnformatted("Layers");
                DrawAnimatorLayerEditor(animatorComp);
            } else if (auto rigidBody = std::dynamic_pointer_cast<gm::physics::RigidBodyComponent>(component)) {
                const char* bodyTypeNames[] = { "Static", "Dynamic" };
                int bodyType = static_cast<int>(rigidBody->GetBodyType());
                if (ImGui::Combo("Body Type", &bodyType, bodyTypeNames, 2)) {
                    rigidBody->SetBodyType(static_cast<gm::physics::RigidBodyComponent::BodyType>(bodyType));
                }

                const char* colliderNames[] = { "Plane", "Box" };
                int colliderShape = static_cast<int>(rigidBody->GetColliderShape());
                if (ImGui::Combo("Collider Shape", &colliderShape, colliderNames, 2)) {
                    rigidBody->SetColliderShape(static_cast<gm::physics::RigidBodyComponent::ColliderShape>(colliderShape));
                }

                if (rigidBody->GetColliderShape() == gm::physics::RigidBodyComponent::ColliderShape::Plane) {
                    glm::vec3 normal = rigidBody->GetPlaneNormal();
                    if (ImGui::DragFloat3("Plane Normal", glm::value_ptr(normal), 0.01f)) {
                        rigidBody->SetPlaneNormal(normal);
                    }
                    float constant = rigidBody->GetPlaneConstant();
                    if (ImGui::DragFloat("Plane Constant", &constant, 0.1f)) {
                        rigidBody->SetPlaneConstant(constant);
                    }
                } else {
                    glm::vec3 halfExtent = rigidBody->GetBoxHalfExtent();
                    if (ImGui::DragFloat3("Box Half Extent", glm::value_ptr(halfExtent), 0.1f)) {
                        rigidBody->SetBoxHalfExtent(halfExtent);
                    }
                }

                if (rigidBody->GetBodyType() == gm::physics::RigidBodyComponent::BodyType::Dynamic) {
                    float mass = rigidBody->GetMass();
                    if (ImGui::DragFloat("Mass", &mass, 0.1f, 0.0f, 1000.0f)) {
                        rigidBody->SetMass(mass);
                    }
                }
            } else if (auto light = std::dynamic_pointer_cast<gm::LightComponent>(component)) {
                const char* lightTypeNames[] = { "Directional", "Point", "Spot" };
                int lightType = static_cast<int>(light->GetType());
                if (ImGui::Combo("Light Type", &lightType, lightTypeNames, 3)) {
                    light->SetType(static_cast<gm::LightComponent::LightType>(lightType));
                }

                bool enabled = light->IsEnabled();
                if (ImGui::Checkbox("Enabled", &enabled)) {
                    light->SetEnabled(enabled);
                }

                glm::vec3 color = light->GetColor();
                if (ImGui::ColorEdit3("Color", glm::value_ptr(color))) {
                    light->SetColor(color);
                }

                float intensity = light->GetIntensity();
                if (ImGui::DragFloat("Intensity", &intensity, 0.1f, 0.0f, 10.0f)) {
                    light->SetIntensity(intensity);
                }

                if (light->GetType() == gm::LightComponent::LightType::Directional ||
                    light->GetType() == gm::LightComponent::LightType::Spot) {
                    glm::vec3 direction = light->GetDirection();
                    if (ImGui::DragFloat3("Direction", glm::value_ptr(direction), 0.01f)) {
                        light->SetDirection(direction);
                    }
                }

                if (light->GetType() == gm::LightComponent::LightType::Point ||
                    light->GetType() == gm::LightComponent::LightType::Spot) {
                    glm::vec3 attenuation = light->GetAttenuation();
                    if (ImGui::DragFloat3("Attenuation", glm::value_ptr(attenuation), 0.001f, 0.0f, 10.0f)) {
                        light->SetAttenuation(attenuation.x, attenuation.y, attenuation.z);
                    }
                    ImGui::Text("(Constant, Linear, Quadratic)");
                }

                if (light->GetType() == gm::LightComponent::LightType::Spot) {
                    float innerDegrees = glm::degrees(light->GetInnerConeAngle());
                    if (ImGui::DragFloat("Inner Cone Angle", &innerDegrees, 1.0f, 0.0f, 90.0f)) {
                        light->SetInnerConeAngle(innerDegrees);
                    }
                    float outerDegrees = glm::degrees(light->GetOuterConeAngle());
                    if (ImGui::DragFloat("Outer Cone Angle", &outerDegrees, 1.0f, 0.0f, 90.0f)) {
                        light->SetOuterConeAngle(outerDegrees);
                    }
                }
            } else if (auto terrain = std::dynamic_pointer_cast<EditableTerrainComponent>(component)) {
                bool editingEnabled = terrain->IsEditingEnabled();
                if (ImGui::Checkbox("Enable Editing", &editingEnabled)) {
                    terrain->SetEditingEnabled(editingEnabled);
                }
                ImGui::Separator();
                const bool isPaintMode = terrain->GetBrushMode() == EditableTerrainComponent::BrushMode::Paint;
                if (isPaintMode) {
                    ImGui::TextUnformatted("Paint Mode: Hold LMB to apply texture, RMB to erase.");
                } else {
                    ImGui::TextUnformatted("Sculpt Mode: Hold LMB to raise terrain, RMB to lower.");
                }
                ImGui::Separator();

                int brushMode = static_cast<int>(terrain->GetBrushMode());
                if (ImGui::RadioButton("Sculpt Height", brushMode == 0)) {
                    terrain->SetBrushMode(EditableTerrainComponent::BrushMode::Sculpt);
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("Paint Texture", brushMode == 1)) {
                    terrain->SetBrushMode(EditableTerrainComponent::BrushMode::Paint);
                }
                brushMode = static_cast<int>(terrain->GetBrushMode());
                ImGui::Separator();

                float brushRadius = terrain->GetBrushRadius();
                if (ImGui::SliderFloat("Brush Radius", &brushRadius,
                        gotmilked::GameConstants::Terrain::BrushRadiusSliderMin,
                        gotmilked::GameConstants::Terrain::BrushRadiusSliderMax, "%.2f m")) {
                    terrain->SetBrushRadius(brushRadius);
                }

                float brushStrength = terrain->GetBrushStrength();
                if (ImGui::SliderFloat("Brush Strength", &brushStrength,
                        gotmilked::GameConstants::Terrain::BrushStrengthSliderMin,
                        gotmilked::GameConstants::Terrain::BrushStrengthSliderMax, "%.2f m/s")) {
                    terrain->SetBrushStrength(brushStrength);
                }
                ImGui::Separator();

                float minHeight = terrain->GetMinHeight();
                if (ImGui::SliderFloat("Min Height", &minHeight,
                        gotmilked::GameConstants::Terrain::MinHeightSliderMin,
                        gotmilked::GameConstants::Terrain::MinHeightSliderMax, "%.2f m")) {
                    terrain->SetMinHeight(minHeight);
                }

                float maxHeight = terrain->GetMaxHeight();
                if (ImGui::SliderFloat("Max Height", &maxHeight,
                        gotmilked::GameConstants::Terrain::MaxHeightSliderMin,
                        gotmilked::GameConstants::Terrain::MaxHeightSliderMax, "%.2f m")) {
                    terrain->SetMaxHeight(maxHeight);
                }

                ImGui::Separator();
                int resolution = terrain->GetResolution();
                if (ImGui::SliderInt("Resolution", &resolution, 2, 512)) {
                    terrain->SetResolution(resolution);
                    terrain->MarkMeshDirty();
                }
                ImGui::Text("Size: %.2f", terrain->GetTerrainSize());

                ImGui::Separator();
                ImGui::TextUnformatted("Base Texture");
                const auto* resources = m_gameResources;
                auto getTextureLabel = [&](const std::string& guid) -> std::string {
                    if (guid.empty()) {
                        return "None";
                    }
                    if (resources) {
                        if (auto source = resources->GetTextureSource(guid)) {
                            if (!source->empty()) {
                                return std::filesystem::path(*source).filename().string();
                            }
                        }
                    }
                    if (auto descriptor = gm::assets::AssetCatalog::Instance().FindByGuid(guid)) {
                        if (!descriptor->relativePath.empty()) {
                            return std::filesystem::path(descriptor->relativePath).filename().string();
                        }
                    }
                    if (resources && resources->GetTexture(guid)) {
                        return guid;
                    }
                    if (auto textureShared = gm::ResourceManager::GetTexture(guid)) {
                        (void)textureShared;
                        return guid;
                    }
                    return "None";
                };

                std::string basePreview = getTextureLabel(terrain->GetBaseTextureGuid());
                if (ImGui::BeginCombo("##TerrainBaseTexture", basePreview.c_str())) {
                    if (ImGui::Selectable("None", terrain->GetBaseTextureGuid().empty())) {
                        terrain->ClearBaseTexture();
                    }
                    if (ImGui::Selectable("Add Texture...", false)) {
                        m_showContentBrowser = true;
                        m_pendingContentBrowserFocusPath = "textures";
                    }
                    ImGui::Separator();
                    if (resources) {
                        for (const auto& [guid, texture] : resources->GetTextureMap()) {
                            const bool isBaseSelected = guid == terrain->GetBaseTextureGuid();
                            std::string label = getTextureLabel(guid);
                            if (ImGui::Selectable(label.c_str(), isBaseSelected)) {
                                if (m_gameResources) {
                                    m_gameResources->EnsureTextureRegistered(guid, texture);
                                }
                                terrain->SetBaseTexture(guid, texture.get());
                            }
                            if (isBaseSelected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                    }
                    ImGui::EndCombo();
                }
                if (ImGui::Button("Clear Base Texture")) {
                    terrain->ClearBaseTexture();
                }

                float textureTiling = terrain->GetTextureTiling();
                if (ImGui::SliderFloat("Texture Tiling", &textureTiling, 0.1f, 32.0f, "%.2f")) {
                    terrain->SetTextureTiling(textureTiling);
                }
                ImGui::TextDisabled("Base texture shows wherever the paint layer is 0.");

                ImGui::Separator();
                ImGui::TextUnformatted("Paint Layers");
                int layerCount = terrain->GetPaintLayerCount();
                int activeLayer = terrain->GetActivePaintLayerIndex();
                int pendingDelete = -1;
                if (ImGui::BeginTable("PaintLayerTable", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
                    ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed, 0.0f);
                    ImGui::TableSetupColumn("Layer", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 0.0f);
                    ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 0.0f);
                    for (int layerIdx = 0; layerIdx < layerCount; ++layerIdx) {
                        ImGui::PushID(layerIdx);
                        ImGui::TableNextRow();

                        bool isLayerActive = (layerIdx == activeLayer);
                        bool enabled = terrain->IsPaintLayerEnabled(layerIdx);
                        std::string label = fmt::format("Layer {}: {}", layerIdx + 1,
                            getTextureLabel(terrain->GetPaintTextureGuid(layerIdx)));

                        ImGui::TableNextColumn();
                        if (ImGui::Checkbox("##LayerEnabled", &enabled)) {
                            terrain->SetPaintLayerEnabled(layerIdx, enabled);
                        }

                        ImGui::TableNextColumn();
                        if (ImGui::Selectable(label.c_str(), isLayerActive, ImGuiSelectableFlags_DontClosePopups)) {
                            terrain->SetActivePaintLayerIndex(layerIdx);
                            activeLayer = terrain->GetActivePaintLayerIndex();
                        }
                        ImGui::SetItemAllowOverlap();

                        ImGui::TableNextColumn();
                        if (terrain->PaintLayerHasPaint(layerIdx)) {
                            ImGui::TextDisabled("painted");
                        } else {
                            ImGui::TextUnformatted(" ");
                        }

                        ImGui::TableNextColumn();
                        ImGui::BeginDisabled(layerCount <= 1);
                        std::string deleteLabel = fmt::format("Delete##{}", layerIdx);
                        if (ImGui::Button(deleteLabel.c_str())) {
                            pendingDelete = layerIdx;
                        }
                        ImGui::EndDisabled();

                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
                if (pendingDelete >= 0) {
                    if (terrain->RemovePaintLayer(pendingDelete)) {
                        layerCount = terrain->GetPaintLayerCount();
                        activeLayer = terrain->GetActivePaintLayerIndex();
                    } else {
                    }
                }
                if (terrain->CanAddPaintLayer()) {
                    if (ImGui::Button("+ Add Layer")) {
                        if (terrain->AddPaintLayer()) {
                            activeLayer = terrain->GetActivePaintLayerIndex();
                            layerCount = terrain->GetPaintLayerCount();
                        } else {
                        }
                    }
                }

                layerCount = terrain->GetPaintLayerCount();
                activeLayer = terrain->GetActivePaintLayerIndex();

                std::string paintPreview = getTextureLabel(terrain->GetPaintTextureGuid(activeLayer));
                if (ImGui::BeginCombo("Layer Texture", paintPreview.c_str())) {
                    if (ImGui::Selectable("None", terrain->GetPaintTextureGuid(activeLayer).empty())) {
                        terrain->ClearPaintTexture();
                    }
                    if (ImGui::Selectable("Add Texture...", false)) {
                        m_showContentBrowser = true;
                        m_pendingContentBrowserFocusPath = "textures";
                    }
                    ImGui::Separator();
                    if (resources) {
                        for (const auto& [guid, texture] : resources->GetTextureMap()) {
                            const bool isPaintSelected = guid == terrain->GetPaintTextureGuid(activeLayer);
                            std::string label = getTextureLabel(guid);
                            if (ImGui::Selectable(label.c_str(), isPaintSelected)) {
                                if (m_gameResources) {
                                    m_gameResources->EnsureTextureRegistered(guid, texture);
                                }
                                terrain->SetPaintTexture(guid, texture.get());
                            }
                            if (isPaintSelected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                    }
                    ImGui::EndCombo();
                }

                const bool hasActivePaintTexture = terrain->PaintLayerHasTexture(activeLayer);
                ImGui::BeginDisabled(!hasActivePaintTexture);
                if (ImGui::Button("Fill Layer With Texture")) {
                    terrain->FillPaintLayer(1.0f);
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Clear Layer (Show Base)")) {
                    terrain->FillPaintLayer(0.0f);
                }
            }
        }
        ImGui::PopID();
        ++componentIndex;
    }
}

void DebugMenu::RenderSceneInfo() {
    auto scene = m_scene.lock();
    if (!scene) {
        if (ImGui::Begin("Scene Info", &m_showSceneInfo)) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No scene available");
            ImGui::End();
        }
        return;
    }

    if (!ImGui::Begin("Scene Info", &m_showSceneInfo)) {
        ImGui::End();
        return;
    }

    ImGui::Text("Scene Name: %s", scene->GetName().c_str());
    ImGui::Text("Initialized: %s", scene->IsInitialized() ? "Yes" : "No");
    ImGui::Text("Paused: %s", scene->IsPaused() ? "Yes" : "No");

    auto allObjects = scene->GetAllGameObjects();
    std::size_t activeCount = 0;
    std::size_t componentCount = 0;
    std::unordered_map<std::string, std::size_t> componentTypes;

    for (const auto& gameObject : allObjects) {
        if (!gameObject || gameObject->IsDestroyed()) {
            continue;
        }
        if (gameObject->IsActive()) {
            ++activeCount;
        }

        auto components = gameObject->GetComponents();
        componentCount += components.size();
        for (const auto& component : components) {
            if (component) {
                std::string compName = component->GetName();
                if (compName.empty()) {
                    compName = "Component";
                }
                componentTypes[compName]++;
            }
        }
    }

    ImGui::Text("GameObjects: %zu", allObjects.size());
    ImGui::Text("Active: %zu", activeCount);
    ImGui::Text("Inactive: %zu", allObjects.size() - activeCount);
    ImGui::Separator();
    ImGui::Text("Total Components: %zu", componentCount);

    if (!componentTypes.empty()) {
        ImGui::Separator();
        ImGui::Text("Component Types:");
        for (const auto& [name, count] : componentTypes) {
            ImGui::BulletText("%s: %zu", name.c_str(), count);
        }
    }

    ImGui::End();
}

void DebugMenu::RenderPrefabBrowser() {
    if (!m_prefabLibrary) {
        m_showPrefabBrowser = false;
        return;
    }

    if (!ImGui::Begin("Prefab Browser", &m_showPrefabBrowser)) {
        ImGui::End();
        return;
    }

    auto names = m_prefabLibrary->GetPrefabNames();
    if (names.empty()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No prefabs found in assets/prefabs");
    } else {
        for (std::size_t i = 0; i < names.size(); ++i) {
            const auto& name = names[i];
            bool isSelected = (m_pendingPrefabToSpawn == name);
            const std::string displayName = name.empty() ? "Unnamed Prefab" : name;
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::Selectable(displayName.c_str(), isSelected)) {
                m_pendingPrefabToSpawn = name;
            }
            ImGui::PopID();
        }
    }

    if (!m_pendingPrefabToSpawn.empty()) {
        ImGui::Separator();
        if (auto* prefab = m_prefabLibrary->GetPrefab(m_pendingPrefabToSpawn)) {
            DrawPrefabDetails(*prefab);
            ImGui::Separator();
        }
        if (ImGui::Button("Spawn Prefab")) {
            auto scene = m_scene.lock();
            if (scene) {
                glm::vec3 cameraPos(0.0f);
                glm::vec3 cameraForward(0.0f, 0.0f, -1.0f);
                if (m_callbacks.getCameraPosition) {
                    cameraPos = m_callbacks.getCameraPosition();
                }
                if (m_callbacks.getCameraForward) {
                    cameraForward = m_callbacks.getCameraForward();
                }
                const glm::vec3 spawnPos = cameraPos + cameraForward * 5.0f;
                auto created = m_prefabLibrary->Instantiate(m_pendingPrefabToSpawn, *scene, spawnPos);
                if (!created.empty()) {
                    auto selectedObject = created.front();
                    m_selectedGameObject = selectedObject;
                    scene->MarkActiveListsDirty();
                    EnsureSelectionWindowsVisible();
                    FocusCameraOnGameObject(selectedObject);
                    gm::core::Logger::Info("[DebugMenu] Spawned prefab '{}'", m_pendingPrefabToSpawn);
                    if (m_applyResourcesCallback) {
                        m_applyResourcesCallback();
                    }
                }
            }
        }
    }

    ImGui::End();
}

void DebugMenu::RenderContentBrowser() {
    if (!m_gameResources) {
        m_showContentBrowser = false;
        return;
    }

    if (!ImGui::Begin("Content Browser", &m_showContentBrowser)) {
        ImGui::End();
        return;
    }

    auto selectedObject = m_selectedGameObject.lock();
    std::shared_ptr<gm::scene::StaticMeshComponent> selectedMeshComp;
    std::shared_ptr<EditableTerrainComponent> selectedTerrainComp;
    if (selectedObject) {
        selectedMeshComp = selectedObject->GetComponent<gm::scene::StaticMeshComponent>();
        selectedTerrainComp = selectedObject->GetComponent<EditableTerrainComponent>();
    }

    if (selectedMeshComp || selectedTerrainComp) {
        ImGui::Text("Assigning to: %s", selectedObject ? selectedObject->GetName().c_str() : "Selection");
        ImGui::SameLine();
        if (selectedTerrainComp && !selectedMeshComp) {
            ImGui::TextDisabled("(double-click textures; hold Shift for base)");
        } else if (selectedTerrainComp && selectedMeshComp) {
            ImGui::TextDisabled("(double-click; Shift+double-click textures for terrain base)");
        } else {
            ImGui::TextDisabled("(double-click to assign)");
        }
    } else {
        ImGui::TextDisabled("Select a GameObject with a StaticMeshComponent or EditableTerrainComponent to assign resources.");
    }

    static char filterBuffer[256] = {0};
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##ContentBrowserFilter", "Filter by name, GUID, or path", filterBuffer, sizeof(filterBuffer));
    std::string filter(filterBuffer);
    std::transform(filter.begin(), filter.end(), filter.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    auto matchesString = [&filter](const std::string& value) {
        if (filter.empty()) {
            return true;
        }
        std::string lowerValue = value;
        std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return lowerValue.find(filter) != std::string::npos;
    };

    auto matchesAsset = [&matchesString](const std::string& guid,
                                         const std::string& name,
                                         const std::string& path) {
        return matchesString(guid) || matchesString(name) ||
               (!path.empty() && matchesString(path));
    };

    // Get assets from catalog, filtered to models, prefabs, shaders, textures, and materials folders
    auto& catalog = gm::assets::AssetCatalog::Instance();
    const auto allAssets = catalog.GetAllAssets();

    std::vector<gm::assets::AssetDescriptor> filteredAssets;
    for (const auto& asset : allAssets) {
        const auto& path = asset.relativePath;
        if (path.rfind("models/", 0) == 0 ||
            path.rfind("prefabs/", 0) == 0 ||
            path.rfind("shaders/", 0) == 0 ||
            path.rfind("textures/", 0) == 0 ||
            path.rfind("materials/", 0) == 0) {
            filteredAssets.push_back(asset);
        }
    }

    // Build folder tree structure
    struct FolderNode {
        std::string name;
        std::string fullPath;
        std::vector<FolderNode> children;
        std::vector<gm::assets::AssetDescriptor> assets;
    };

    FolderNode root;
    root.name = "Assets";
    root.fullPath = "";

    auto ensurePath = [&root](const std::filesystem::path& relativePath) -> FolderNode* {
        FolderNode* current = &root;
        for (const auto& part : relativePath) {
            if (part == "." || part.empty()) {
                continue;
            }
            std::string partStr = part.string();
            auto it = std::find_if(current->children.begin(), current->children.end(),
                                    [&partStr](const FolderNode& node) { return node.name == partStr; });
            if (it == current->children.end()) {
                FolderNode newFolder;
                newFolder.name = partStr;
                newFolder.fullPath = current->fullPath.empty() ? partStr : (current->fullPath + "/" + partStr);
                current->children.push_back(newFolder);
                current = &current->children.back();
            } else {
                current = &*it;
            }
        }
        return current;
    };

    for (const auto& asset : filteredAssets) {
        std::filesystem::path assetPath(asset.relativePath);
        auto parentPath = assetPath.parent_path();
        FolderNode* folder = ensurePath(parentPath);
        folder->assets.push_back(asset);
    }

    // Sort folders and assets
    std::function<void(FolderNode&)> sortNode = [&sortNode](FolderNode& node) {
        std::sort(node.children.begin(), node.children.end(),
                  [](const FolderNode& a, const FolderNode& b) { return a.name < b.name; });
        std::sort(node.assets.begin(), node.assets.end(),
                  [](const gm::assets::AssetDescriptor& a, const gm::assets::AssetDescriptor& b) {
                      return a.relativePath < b.relativePath;
                  });
        for (auto& child : node.children) {
            sortNode(child);
        }
    };
    sortNode(root);

    auto assetTypeToString = [](const gm::assets::AssetDescriptor& asset) -> const char* {
        const std::string& path = asset.relativePath;
        if (asset.type == gm::assets::AssetType::Mesh || path.rfind("models/", 0) == 0) return "Mesh";
        if (asset.type == gm::assets::AssetType::Shader || path.rfind("shaders/", 0) == 0) return "Shader";
        if (asset.type == gm::assets::AssetType::Texture || path.rfind("textures/", 0) == 0) return "Texture";
        if (asset.type == gm::assets::AssetType::Material || path.rfind("materials/", 0) == 0) return "Material";
        if (asset.type == gm::assets::AssetType::Prefab || path.rfind("prefabs/", 0) == 0) return "Prefab";
        if (asset.type == gm::assets::AssetType::Scene) return "Scene";
        if (asset.type == gm::assets::AssetType::Audio) return "Audio";
        if (asset.type == gm::assets::AssetType::Script) return "Script";
        return "Asset";
    };

    // Helper to get or load assets on-demand
    auto getOrLoadMesh = [this](const gm::assets::AssetDescriptor& asset) -> gm::Mesh* {
        // First check if already loaded in GameResources
        if (auto* mesh = m_gameResources->GetMesh(asset.guid)) {
            return mesh;
        }
        
        // Try ResourceManager directly (it may have been loaded globally)
        if (auto mesh = gm::ResourceManager::GetMesh(asset.guid)) {
            return mesh.get();
        }
        
        // Try to load on-demand
        try {
            auto& catalog = gm::assets::AssetCatalog::Instance();
            const auto assetRoot = catalog.GetAssetRoot();
            auto path = (assetRoot / asset.relativePath).lexically_normal();
            gm::ResourceManager::MeshDescriptor desc{asset.guid, path.string()};
            auto handle = gm::ResourceManager::LoadMesh(desc);
            if (handle.IsLoaded()) {
                auto mesh = handle.Lock();
                gm::core::Logger::Info("[ContentBrowser] Loaded mesh '{}' on-demand", asset.guid);
                return mesh.get();
            }
        } catch (const std::exception& ex) {
            gm::core::Logger::Warning("[ContentBrowser] Failed to load mesh '{}' on-demand: {}", asset.guid, ex.what());
        }
        
        return nullptr;
    };

    auto getOrLoadTexture = [this](const gm::assets::AssetDescriptor& asset) -> std::shared_ptr<gm::Texture> {
        if (auto texture = gm::ResourceManager::GetTexture(asset.guid)) {
            return texture;
        }

        if (m_gameResources->GetTexture(asset.guid)) {
            if (auto texture = gm::ResourceManager::GetTexture(asset.guid)) {
                return texture;
            }
        }

        try {
            auto& catalog = gm::assets::AssetCatalog::Instance();
            const auto assetRoot = catalog.GetAssetRoot();
            auto path = (assetRoot / asset.relativePath).lexically_normal();
            gm::ResourceManager::TextureDescriptor desc{
                asset.guid,
                path.string(),
                true,
                true,
                true
            };
            auto handle = gm::ResourceManager::LoadTexture(desc);
            if (handle.IsLoaded()) {
                auto texture = handle.Lock();
                gm::core::Logger::Info("[ContentBrowser] Loaded texture '{}' on-demand", asset.guid);
                return texture;
            }
        } catch (const std::exception& ex) {
            gm::core::Logger::Warning("[ContentBrowser] Failed to load texture '{}' on-demand: {}", asset.guid, ex.what());
        }

        return {};
    };

    // Render folder tree
    const std::string focusPath = m_pendingContentBrowserFocusPath;
    bool focusHandled = false;

    std::function<void(const FolderNode&, int)> renderFolder = [&](const FolderNode& folder, int depth) {
        // Skip empty folders (except root which we'll handle specially)
        if (folder.children.empty() && folder.assets.empty() && folder.name != "Assets") {
            return;
        }

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        if (folder.children.empty() && folder.assets.empty()) {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }
        
        // For root "Assets", use default open state; for others, start collapsed
        if (folder.name == "Assets") {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }

        if (!focusPath.empty()) {
            if (folder.fullPath == focusPath ||
                (!folder.fullPath.empty() && focusPath.rfind(folder.fullPath + "/", 0) == 0)) {
                ImGui::SetNextItemOpen(true, ImGuiCond_Always);
            }
        }

        bool isOpen = ImGui::TreeNodeEx(folder.name.c_str(), flags);
        
        if (isOpen) {
            // Render child folders
            for (const auto& child : folder.children) {
                renderFolder(child, depth + 1);
            }

            // Render assets in this folder
            for (const auto& asset : folder.assets) {
                if (!focusHandled && !focusPath.empty() && folder.fullPath == focusPath) {
                    ImGui::SetScrollHereY();
                    focusHandled = true;
                }
                const std::string fileName = std::filesystem::path(asset.relativePath).filename().string();
                if (!matchesAsset(asset.guid, fileName, asset.relativePath)) {
                    continue;
                }

                ImGui::PushID(asset.guid.c_str());
                
                // TODO: Add icon/preview support here
                // For now, use a simple text label
                const char* icon = "📄";
                const bool isMeshAsset = asset.type == gm::assets::AssetType::Mesh || asset.relativePath.rfind("models/", 0) == 0;
                const bool isShaderAsset = asset.type == gm::assets::AssetType::Shader || asset.relativePath.rfind("shaders/", 0) == 0;
                const bool isTextureAsset = asset.type == gm::assets::AssetType::Texture || asset.relativePath.rfind("textures/", 0) == 0;
                const bool isMaterialAsset = asset.type == gm::assets::AssetType::Material || asset.relativePath.rfind("materials/", 0) == 0;
                const bool isPrefabAsset = asset.type == gm::assets::AssetType::Prefab || asset.relativePath.rfind("prefabs/", 0) == 0;

                if (isMeshAsset) {
                    icon = "📦";
                } else if (isShaderAsset) {
                    icon = "🔧";
                } else if (isPrefabAsset) {
                    icon = "🎯";
                } else if (isTextureAsset) {
                    icon = "🖼️";
                } else if (isMaterialAsset) {
                    icon = "🧪";
                }

                bool isSelected = false;
                if (selectedMeshComp) {
                    if (isMeshAsset) {
                        isSelected = selectedMeshComp->GetMeshGuid() == asset.guid;
                    } else if (isShaderAsset) {
                        isSelected = selectedMeshComp->GetShaderGuid() == asset.guid;
                    } else if (isMaterialAsset) {
                        isSelected = selectedMeshComp->GetMaterialGuid() == asset.guid;
                    } else if (isTextureAsset) {
                        if (auto material = selectedMeshComp->GetMaterial()) {
                            if (auto* tex = m_gameResources->GetTexture(asset.guid)) {
                                isSelected = material->GetDiffuseTexture() == tex;
                            } else if (auto texPtr = gm::ResourceManager::GetTexture(asset.guid)) {
                                isSelected = material->GetDiffuseTexture() == texPtr.get();
                            }
                        }
                    }
                }
                if (!isSelected && selectedTerrainComp && isTextureAsset) {
                    const bool matchesBase = asset.guid == selectedTerrainComp->GetBaseTextureGuid();
                    bool matchesPaint = false;
                    for (int layerIdx = 0; layerIdx < selectedTerrainComp->GetPaintLayerCount(); ++layerIdx) {
                        if (asset.guid == selectedTerrainComp->GetPaintTextureGuid(layerIdx)) {
                            matchesPaint = true;
                            break;
                        }
                    }
                    isSelected = matchesBase || matchesPaint;
                }

                if (isSelected) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
                }

                ImGui::Selectable((std::string(icon) + " " + fileName).c_str(), isSelected, 
                                  ImGuiSelectableFlags_AllowDoubleClick);

                if (isSelected) {
                    ImGui::PopStyleColor();
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("GUID: %s", asset.guid.c_str());
                    ImGui::Text("Path: %s", asset.relativePath.c_str());
                    ImGui::Text("Type: %s", assetTypeToString(asset));
                    if (selectedTerrainComp && isTextureAsset) {
                        const bool isBase = asset.guid == selectedTerrainComp->GetBaseTextureGuid();
                        if (isBase) {
                            ImGui::TextUnformatted("Status: Base Texture");
                        }
                        std::vector<int> matchingLayers;
                        for (int layerIdx = 0; layerIdx < selectedTerrainComp->GetPaintLayerCount(); ++layerIdx) {
                            if (asset.guid == selectedTerrainComp->GetPaintTextureGuid(layerIdx)) {
                                matchingLayers.push_back(layerIdx);
                            }
                        }
                        if (!matchingLayers.empty()) {
                            std::string layerText = "Status: Paint Layer ";
                            for (std::size_t i = 0; i < matchingLayers.size(); ++i) {
                                layerText += std::to_string(matchingLayers[i] + 1);
                                if (i + 1 < matchingLayers.size()) {
                                    layerText += ", ";
                                }
                            }
                            ImGui::TextUnformatted(layerText.c_str());
                        }
                        ImGui::TextUnformatted("Tip: Double-click to assign paint; hold Shift for base.");
                    }
                    ImGui::EndTooltip();
                }

                // Double-click to assign
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
                    (selectedMeshComp || selectedTerrainComp)) {
                    bool assigned = false;
                    
                    if (selectedMeshComp && isMeshAsset) {
                        auto* mesh = getOrLoadMesh(asset);
                        if (mesh) {
                            selectedMeshComp->SetMesh(mesh, asset.guid);
                            assigned = true;
                            gm::core::Logger::Info("[ContentBrowser] Assigned mesh '{}' to '{}'", 
                                asset.guid, selectedObject ? selectedObject->GetName().c_str() : "(none)");
                        }
                    } else if (selectedMeshComp && isShaderAsset) {
                        auto* shader = m_gameResources->GetShader(asset.guid);
                        if (shader) {
                            selectedMeshComp->SetShader(shader, asset.guid);
                            shader->Use();
                            shader->SetInt("uTex", 0);
                            assigned = true;
                            gm::core::Logger::Info("[ContentBrowser] Assigned shader '{}' to '{}'", 
                                asset.guid, selectedObject ? selectedObject->GetName().c_str() : "(none)");
                        }
                    } else if (selectedMeshComp && isTextureAsset) {
                        if (auto material = selectedMeshComp->GetMaterial()) {
                            if (auto texture = getOrLoadTexture(asset)) {
                                m_gameResources->EnsureTextureRegistered(asset.guid, texture);
                                material->SetDiffuseTexture(texture.get());
                                assigned = true;
                                gm::core::Logger::Info("[ContentBrowser] Assigned texture '{}' to material '{}' on '{}'",
                                    asset.guid,
                                    material->GetName().c_str(),
                                    selectedObject ? selectedObject->GetName().c_str() : "(none)");
                            }
                        }
                    } else if (selectedMeshComp && isMaterialAsset) {
                        if (auto material = m_gameResources->GetMaterial(asset.guid)) {
                            selectedMeshComp->SetMaterial(material, asset.guid);
                            assigned = true;
                            gm::core::Logger::Info("[ContentBrowser] Assigned material '{}' to '{}'",
                                asset.guid,
                                selectedObject ? selectedObject->GetName().c_str() : "(none)");
                        }
                    }

                    if (!assigned && selectedTerrainComp && isTextureAsset) {
                        if (auto texture = getOrLoadTexture(asset)) {
                            ImGuiIO& io = ImGui::GetIO();
                            const bool assignAsBase = (io.KeyMods & ImGuiMod_Shift) != 0;
                            if (assignAsBase) {
                                m_gameResources->EnsureTextureRegistered(asset.guid, texture);
                                selectedTerrainComp->SetBaseTexture(asset.guid, texture.get());
                                gm::core::Logger::Info("[ContentBrowser] Assigned base texture '{}' to '{}'",
                                    asset.guid,
                                    selectedObject ? selectedObject->GetName().c_str() : "(none)");
                            } else {
                                m_gameResources->EnsureTextureRegistered(asset.guid, texture);
                                selectedTerrainComp->SetPaintTexture(asset.guid, texture.get());
                                gm::core::Logger::Info("[ContentBrowser] Assigned paint texture '{}' to '{}'",
                                    asset.guid,
                                    selectedObject ? selectedObject->GetName().c_str() : "(none)");
                            }
                            assigned = true;
                        }
                    }

                    const bool relevantToTerrain = selectedTerrainComp && isTextureAsset;
                    const bool relevantToMesh = static_cast<bool>(selectedMeshComp);
                    if (!assigned && (relevantToMesh || relevantToTerrain)) {
                        gm::core::Logger::Warning("[ContentBrowser] Failed to assign asset '%s' (not loaded)", asset.guid.c_str());
                    }
                }

                ImGui::PopID();
            }

            ImGui::TreePop();
        }
    };

    if (filteredAssets.empty()) {
        ImGui::TextDisabled("No assets found in models/, prefabs/, shaders/, textures/, or materials/ folders.");
    } else {
        ImGui::BeginChild("ContentBrowserTree", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        renderFolder(root, 0);
        ImGui::EndChild();
    }

    m_pendingContentBrowserFocusPath.clear();

    ImGui::End();
}

void DebugMenu::RenderTransformGizmo() {
    if (ShouldDelaySceneUI()) {
        return;
    }

    auto scene = m_scene.lock();
    if (!scene) {
        return;
    }

    auto selected = m_selectedGameObject.lock();
    if (!selected) {
        return;
    }

    auto transform = selected->GetTransform();
    if (!transform) {
        return;
    }

    if (!m_callbacks.getViewMatrix || !m_callbacks.getProjectionMatrix) {
        return;
    }

    if (!m_showSceneExplorer) {
        return;
    }

    ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    if (!mainViewport) {
        return;
    }

    ImGuizmo::BeginFrame();
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
    ImGuizmo::SetRect(mainViewport->Pos.x, mainViewport->Pos.y, mainViewport->Size.x, mainViewport->Size.y);

    glm::mat4 view = m_callbacks.getViewMatrix();
    glm::mat4 projection = m_callbacks.getProjectionMatrix();
    glm::mat4 model = transform->GetMatrix();

    ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
    if (m_gizmoOperation == 1) {
        operation = ImGuizmo::ROTATE;
    } else if (m_gizmoOperation == 2) {
        operation = ImGuizmo::SCALE;
    }

    ImGuizmo::MODE mode = (m_gizmoMode == 1) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

    ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), operation, mode, glm::value_ptr(model));
    if (ImGuizmo::IsUsing()) {
        glm::vec3 translation;
        glm::vec3 rotation;
        glm::vec3 scale;
        ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(model), glm::value_ptr(translation), glm::value_ptr(rotation), glm::value_ptr(scale));
        transform->SetPosition(translation);
        transform->SetRotation(rotation);
        transform->SetScale(scale);
        scene->MarkActiveListsDirty();
    }
}

void DebugMenu::RenderAnimationDebugger() {
    if (!ImGui::Begin("Animation Preview", &m_showAnimationDebugger)) {
        ImGui::End();
        return;
    }

    if (!m_gameResources) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Game resources unavailable.");
        ImGui::End();
        return;
    }

    if (ImGui::Button("Refresh Asset List")) {
        m_animationAssetsDirty = true;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Bone Overlay", &m_enableBoneOverlay);
    ImGui::SameLine();
    ImGui::Checkbox("Animation HUD", &m_showAnimationDebugOverlay);

    if (ImGui::CollapsingHeader("Stress Tools", ImGuiTreeNodeFlags_DefaultOpen)) {
        static int herdColumns = 8;
        static int herdRows = 6;
        static float herdSpacing = 2.5f;
        static float herdOriginY = 0.0f;

        ImGui::SliderInt("Columns", &herdColumns, 1, 64);
        ImGui::SliderInt("Rows", &herdRows, 1, 64);
        ImGui::SliderFloat("Spacing", &herdSpacing, 0.5f, 10.0f, "%.1f");
        ImGui::SliderFloat("Ground Offset Y", &herdOriginY, -2.0f, 2.0f, "%.2f");

        if (ImGui::Button("Spawn Cow Herd")) {
            const float extentX = (std::max(herdColumns - 1, 0) * herdSpacing) * 0.5f;
            const float extentZ = (std::max(herdRows - 1, 0) * herdSpacing) * 0.5f;
            glm::vec3 origin(-extentX, herdOriginY, -extentZ);
            SpawnCowHerd(herdColumns, herdRows, herdSpacing, origin);
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("Instantiates the 'Cow' prefab in a grid.");
    }

    ImGui::InputTextWithHint("Filter", "substring match", m_animationFilterBuffer.data(), m_animationFilterBuffer.size());

    EnsureAnimationAssetCache();

    std::string filterLower(m_animationFilterBuffer.data());
    std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    auto matchesFilter = [&](const std::string& label) {
        if (filterLower.empty()) {
            return true;
        }
        std::string lowered = label;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return lowered.find(filterLower) != std::string::npos;
    };

    ImVec2 tableAvail = ImGui::GetContentRegionAvail();
    float skeletonColumnWidth = tableAvail.x * 0.45f;
    skeletonColumnWidth = std::clamp(skeletonColumnWidth, 200.0f, tableAvail.x - 200.0f);

    if (ImGui::BeginChild("SkeletonColumn", ImVec2(skeletonColumnWidth, 0), false, ImGuiWindowFlags_NoScrollbar)) {
        ImGui::Text("Skeleton Assets (%zu)", m_animationSkeletonAssets.size());
        ImGui::Separator();
        ImGui::BeginChild("SkeletonAssetList", ImVec2(0, 220), true, ImGuiWindowFlags_HorizontalScrollbar);
        for (const auto& entry : m_animationSkeletonAssets) {
            if (!matchesFilter(entry.displayName)) {
                continue;
            }
            bool selectedEntry = (m_selectedSkeletonAsset == entry.displayName);
            if (ImGui::Selectable(entry.displayName.c_str(), selectedEntry)) {
                if (LoadPreviewSkeleton(entry)) {
                    m_selectedSkeletonAsset = entry.displayName;
                }
            }
        }
        ImGui::EndChild();

        if (m_previewSkeleton) {
            ImGui::Separator();
            ImGui::Text("Bone Count: %zu", m_previewSkeleton->bones.size());
            const bool skeletonHierarchyOpen = ImGui::BeginChild("SkeletonHierarchy", ImVec2(0, 200), true);
            if (skeletonHierarchyOpen) {
                const std::size_t boneCount = m_previewSkeleton->bones.size();
                std::vector<std::vector<int>> children(boneCount);
                for (std::size_t i = 0; i < boneCount; ++i) {
                    int parent = m_previewSkeleton->bones[i].parentIndex;
                    if (parent >= 0 && static_cast<std::size_t>(parent) < boneCount) {
                        children[static_cast<std::size_t>(parent)].push_back(static_cast<int>(i));
                    }
                }
                std::function<void(int)> drawNode = [&](int index) {
                    const auto& bone = m_previewSkeleton->bones[index];
                    std::string label = fmt::format("{} ({})",
                                                    bone.name.empty() ? fmt::format("Bone {}", index) : bone.name,
                                                    index);
                    if (ImGui::TreeNode(label.c_str())) {
                        ImGui::Text("Parent: %d", bone.parentIndex);
                        if (m_previewClip && index < static_cast<int>(m_previewPose.Size())) {
                            const auto& transform = m_previewPose.LocalTransform(static_cast<std::size_t>(index));
                            ImGui::Text("Translation: (%.2f, %.2f, %.2f)",
                                        transform.translation.x, transform.translation.y, transform.translation.z);
                            ImGui::Text("Scale: (%.2f, %.2f, %.2f)",
                                        transform.scale.x, transform.scale.y, transform.scale.z);
                        }
                        for (int child : children[static_cast<std::size_t>(index)]) {
                            drawNode(child);
                        }
                        ImGui::TreePop();
                    }
                };
                for (std::size_t i = 0; i < boneCount; ++i) {
                    if (m_previewSkeleton->bones[i].parentIndex < 0) {
                        drawNode(static_cast<int>(i));
                    }
                }
                if (boneCount == 0) {
                    ImGui::TextDisabled("No bones in skeleton.");
                }
            }
            ImGui::EndChild();
        } else {
            ImGui::Separator();
            ImGui::TextDisabled("Select a skeleton to view hierarchy.");
        }
        ImGui::EndChild();
    }

    ImGui::SameLine();

    if (ImGui::BeginChild("ClipColumn", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar)) {
        ImGui::Text("Clip Assets (%zu)", m_animationClipAssets.size());
        ImGui::Separator();
        ImGui::BeginChild("ClipAssetList", ImVec2(0, 220), true, ImGuiWindowFlags_HorizontalScrollbar);
        for (const auto& entry : m_animationClipAssets) {
            if (!matchesFilter(entry.displayName)) {
                continue;
            }
            bool selectedEntry = (m_selectedClipAsset == entry.displayName);
            if (ImGui::Selectable(entry.displayName.c_str(), selectedEntry)) {
                if (LoadPreviewClip(entry)) {
                    m_selectedClipAsset = entry.displayName;
                }
            }
        }
        ImGui::EndChild();

        if (m_previewClip) {
            const double clipDurationSec = (m_previewClip->ticksPerSecond > 0.0)
                                               ? (m_previewClip->duration / m_previewClip->ticksPerSecond)
                                               : m_previewClip->duration;
            ImGui::Separator();
            ImGui::Text("Duration: %.3fs  Channels: %zu", clipDurationSec, m_previewClip->channels.size());
            float previewTime = static_cast<float>(m_previewTimeSeconds);
            if (clipDurationSec > 0.0) {
                if (ImGui::SliderFloat("Preview Time", &previewTime, 0.0f, static_cast<float>(clipDurationSec))) {
                    m_previewTimeSeconds = previewTime;
                    RefreshAnimationPreviewPose();
                }
            } else {
                ImGui::TextDisabled("Clip has zero duration.");
            }

            if (ImGui::Checkbox("Loop Preview", &m_previewLoop)) {
                // no-op, flag stored
            }
            ImGui::SameLine();
            if (ImGui::Button(m_previewPlaying ? "Pause" : "Play")) {
                m_previewPlaying = !m_previewPlaying;
            }

            if (m_previewPlaying && clipDurationSec > 0.0) {
                m_previewTimeSeconds += ImGui::GetIO().DeltaTime;
                if (m_previewLoop) {
                    m_previewTimeSeconds = std::fmod(m_previewTimeSeconds, clipDurationSec);
                } else {
                    m_previewTimeSeconds = std::min(m_previewTimeSeconds, clipDurationSec);
                }
                RefreshAnimationPreviewPose();
            }

            if (m_previewSkeleton) {
                ImGui::Separator();
                ImGui::TextUnformatted("Skeleton Preview");
                ImVec2 canvasSize = ImVec2(ImGui::GetContentRegionAvail().x, 220.0f);
                if (canvasSize.x < 50.0f) canvasSize.x = 50.0f;
                if (canvasSize.y < 120.0f) canvasSize.y = 120.0f;
                ImGui::BeginChild(
                    "SkeletonPreviewArea", canvasSize, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                ImGui::InvisibleButton("SkeletonPreviewCanvas", canvasSize, ImGuiButtonFlags_MouseButtonLeft);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetItemUsingMouseWheel();
                }
                DrawPreviewSkeleton(canvasSize);
                ImGui::EndChild();

                const bool previewBonesOpen = ImGui::BeginChild("PreviewBonesList", ImVec2(0, 160), true);
                if (previewBonesOpen) {
                    ImGui::Columns(4, "PreviewBoneColumns");
                    ImGui::TextUnformatted("Bone");
                    ImGui::NextColumn();
                    ImGui::TextUnformatted("Translation");
                    ImGui::NextColumn();
                    ImGui::TextUnformatted("Rotation");
                    ImGui::NextColumn();
                    ImGui::TextUnformatted("Scale");
                    ImGui::NextColumn();
                    ImGui::Separator();

                    const std::size_t boneCount = std::min<std::size_t>(m_previewSkeleton->bones.size(), 32);
                    for (std::size_t i = 0; i < boneCount; ++i) {
                        const auto& bone = m_previewSkeleton->bones[i];
                        const auto& transform = m_previewPose.LocalTransform(i);
                        ImGui::Text("%s (%zu)", bone.name.empty() ? "<unnamed>" : bone.name.c_str(), i);
                        ImGui::NextColumn();
                        ImGui::Text("%.2f %.2f %.2f",
                                    transform.translation.x, transform.translation.y, transform.translation.z);
                        ImGui::NextColumn();
                        ImGui::Text("%.2f %.2f %.2f %.2f",
                                    transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w);
                        ImGui::NextColumn();
                        ImGui::Text("%.2f %.2f %.2f",
                                    transform.scale.x, transform.scale.y, transform.scale.z);
                        ImGui::NextColumn();
                    }
                    if (m_previewSkeleton->bones.size() > boneCount) {
                        ImGui::TextDisabled("... (%zu more)", m_previewSkeleton->bones.size() - boneCount);
                    }

                    ImGui::Columns(1);
                }
                ImGui::EndChild();
            } else {
                ImGui::TextDisabled("Select a skeleton to evaluate clip pose.");
            }
        } else {
            ImGui::Separator();
            ImGui::TextDisabled("Select an animation clip to preview.");
        }
        ImGui::EndChild();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Overlay Settings");
    ImGui::Checkbox("Show Bone Names", &m_showBoneNames);
    ImGui::Checkbox("Draw Bones On All Objects", &m_boneOverlayAllObjects);
    ImGui::SliderFloat("Bone Marker Radius", &m_boneOverlayNodeRadius, 2.0f, 12.0f);
    ImGui::SliderFloat("Bone Line Thickness", &m_boneOverlayLineThickness, 1.0f, 6.0f);

    ImGui::End();
}

void DebugMenu::DrawAnimatorLayerEditor(const std::shared_ptr<gm::scene::AnimatorComponent>& animator) {
    if (!animator) {
        ImGui::TextDisabled("Animator not available.");
        return;
    }

    auto snapshots = animator->GetLayerSnapshots();
    if (snapshots.empty()) {
        ImGui::TextDisabled("No animation layers configured.");
        return;
    }

    EnsureAnimationAssetCache();

    for (auto& snapshot : snapshots) {
        ImGui::PushID(snapshot.slot.c_str());
        if (ImGui::TreeNode(snapshot.slot.c_str())) {
            bool playing = snapshot.playing;
            if (ImGui::Checkbox("Playing", &playing)) {
                snapshot.playing = playing;
                if (playing) {
                    animator->Play(snapshot.slot, snapshot.loop);
                } else {
                    animator->Stop(snapshot.slot);
                }
            }

            bool loop = snapshot.loop;
            if (ImGui::Checkbox("Loop", &loop)) {
                snapshot.loop = loop;
                animator->ApplyLayerSnapshot(snapshot);
            }

            float weight = snapshot.weight;
            if (ImGui::SliderFloat("Weight", &weight, 0.0f, 1.0f)) {
                snapshot.weight = weight;
                animator->SetWeight(snapshot.slot, weight);
            }

            float timeSeconds = static_cast<float>(snapshot.timeSeconds);
            if (ImGui::DragFloat("Time (s)", &timeSeconds, 0.01f, 0.0f, 1000.0f)) {
                snapshot.timeSeconds = timeSeconds;
                animator->ApplyLayerSnapshot(snapshot);
            }

            const char* clipPreview = snapshot.clipGuid.empty() ? "None" : snapshot.clipGuid.c_str();
            if (!m_animationClipAssets.empty() &&
                ImGui::BeginCombo("Clip Asset", clipPreview)) {
                for (const auto& entry : m_animationClipAssets) {
                    bool clipSelected = (snapshot.clipGuid == entry.displayName);
                    if (ImGui::Selectable(entry.displayName.c_str(), clipSelected)) {
                        AssignClipToLayer(*animator, snapshot.slot, entry);
                        snapshot.clipGuid = entry.displayName;
                        animator->ApplyLayerSnapshot(snapshot);
                    }
                    if (clipSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            } else if (m_animationClipAssets.empty()) {
                ImGui::TextDisabled("No animation clip assets detected.");
            }

            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void DebugMenu::EnsureAnimationAssetCache() {
    if (!m_animationAssetsDirty) {
        return;
    }
    m_animationAssetsDirty = false;
    m_animationSkeletonAssets.clear();
    m_animationClipAssets.clear();

    if (!m_gameResources) {
        return;
    }

    const auto& root = m_gameResources->GetAssetsDirectory();
    if (root.empty()) {
        return;
    }

    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator it(root, ec), end; it != end && !ec; ++it) {
        if (!it->is_regular_file()) {
            continue;
        }
        auto ext = it->path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        AnimationAssetEntry entry{it->path(), RelativeAssetLabel(it->path())};
        if (ext == ".gmskel") {
            m_animationSkeletonAssets.push_back(entry);
        } else if (ext == ".gmanim") {
            m_animationClipAssets.push_back(entry);
        }
    }

    if (ec) {
        gm::core::Logger::Warning("[DebugMenu] Animation asset scan error: {}", ec.message());
    }

    auto sorter = [](const AnimationAssetEntry& a, const AnimationAssetEntry& b) {
        return a.displayName < b.displayName;
    };
    std::sort(m_animationSkeletonAssets.begin(), m_animationSkeletonAssets.end(), sorter);
    std::sort(m_animationClipAssets.begin(), m_animationClipAssets.end(), sorter);
}

std::string DebugMenu::RelativeAssetLabel(const std::filesystem::path& absolute) const {
    if (m_gameResources) {
        const auto& root = m_gameResources->GetAssetsDirectory();
        if (!root.empty()) {
            std::error_code ec;
            auto relative = std::filesystem::relative(absolute, root, ec);
            if (!ec) {
                auto label = relative.generic_string();
                if (!label.empty()) {
                    return label;
                }
            }
        }
    }
    return absolute.filename().generic_string();
}

bool DebugMenu::LoadPreviewSkeleton(const AnimationAssetEntry& entry) {
    try {
        auto skeletonData = gm::animation::Skeleton::FromFile(entry.absolutePath.string());
        m_previewSkeleton = std::make_shared<gm::animation::Skeleton>(std::move(skeletonData));
        m_previewEvaluator = std::make_unique<gm::animation::AnimationPoseEvaluator>(*m_previewSkeleton);
        m_previewPose.Resize(m_previewSkeleton->bones.size());
        m_previewTimeSeconds = 0.0;
        RemapPreviewClip();
        RefreshAnimationPreviewPose();
        return true;
    } catch (const std::exception& ex) {
        gm::core::Logger::Error("[DebugMenu] Failed to load skeleton '{}': {}", entry.displayName, ex.what());
    }
    return false;
}

bool DebugMenu::LoadPreviewClip(const AnimationAssetEntry& entry) {
    try {
        auto clip = std::make_unique<gm::animation::AnimationClip>(gm::animation::AnimationClip::FromFile(entry.absolutePath.string()));
        m_previewClip = std::move(clip);
        m_previewTimeSeconds = 0.0;
        RemapPreviewClip();
        RefreshAnimationPreviewPose();
        return true;
    } catch (const std::exception& ex) {
        gm::core::Logger::Error("[DebugMenu] Failed to load animation '{}': {}", entry.displayName, ex.what());
    }
    return false;
}

void DebugMenu::RemapPreviewClip() {
    if (!m_previewSkeleton || !m_previewClip) {
        return;
    }

    for (auto& channel : m_previewClip->channels) {
        channel.boneIndex = m_previewSkeleton->FindBoneIndex(channel.boneName);
    }
}

void DebugMenu::RefreshAnimationPreviewPose() {
    if (!m_previewEvaluator || !m_previewClip || !m_previewSkeleton) {
        m_previewBoneMatrices.clear();
        return;
    }

    const double clipDurationSec = (m_previewClip->ticksPerSecond > 0.0)
                                       ? (m_previewClip->duration / m_previewClip->ticksPerSecond)
                                       : m_previewClip->duration;
    if (clipDurationSec > 0.0) {
        m_previewTimeSeconds = std::fmod(std::max(0.0, m_previewTimeSeconds), clipDurationSec);
    } else {
        m_previewTimeSeconds = 0.0;
    }
    m_previewEvaluator->EvaluateClip(*m_previewClip, m_previewTimeSeconds, m_previewPose);

    const std::size_t boneCount = m_previewSkeleton->bones.size();
    m_previewBoneMatrices.resize(boneCount);
    m_previewPose.BuildLocalMatrices();
    const auto& locals = m_previewPose.LocalMatrices();
    for (std::size_t i = 0; i < boneCount; ++i) {
        glm::mat4 global = locals[i];
        const int parent = m_previewSkeleton->bones[i].parentIndex;
        if (parent >= 0 && static_cast<std::size_t>(parent) < boneCount) {
            global = m_previewBoneMatrices[static_cast<std::size_t>(parent)] * global;
        }
        m_previewBoneMatrices[i] = global;
    }
}

void DebugMenu::AssignSkeletonFromAsset(gm::scene::AnimatorComponent& animator, const AnimationAssetEntry& entry) {
    gm::ResourceManager::SkeletonDescriptor desc;
    desc.guid = entry.displayName;
    desc.path = entry.absolutePath.string();
    auto handle = gm::ResourceManager::LoadSkeleton(desc);
    animator.SetSkeleton(std::move(handle));
}

void DebugMenu::AssignClipToLayer(gm::scene::AnimatorComponent& animator,
                                  const std::string& slot,
                                  const AnimationAssetEntry& entry) {
    gm::ResourceManager::AnimationClipDescriptor desc;
    desc.guid = entry.displayName;
    desc.path = entry.absolutePath.string();
    auto handle = gm::ResourceManager::LoadAnimationClip(desc);
    animator.SetClip(slot, std::move(handle));
}

void DebugMenu::SpawnCowHerd(int columns, int rows, float spacing, const glm::vec3& origin) {
    columns = std::max(columns, 1);
    rows = std::max(rows, 1);
    spacing = std::max(spacing, 0.1f);

    if (!m_prefabLibrary) {
        gm::core::Logger::Warning("[DebugMenu] PrefabLibrary unavailable; cannot spawn herd");
        return;
    }

    auto scene = m_scene.lock();
    if (!scene) {
        gm::core::Logger::Warning("[DebugMenu] Scene unavailable; cannot spawn herd");
        return;
    }

    std::size_t spawnedCount = 0;
    for (int z = 0; z < rows; ++z) {
        for (int x = 0; x < columns; ++x) {
            const glm::vec3 offset(static_cast<float>(x) * spacing, 0.0f, static_cast<float>(z) * spacing);
            const glm::vec3 position = origin + offset;
            auto instances = m_prefabLibrary->Instantiate("Cow", *scene, position);
            spawnedCount += instances.size();
        }
    }

    gm::core::Logger::Info(
        "[DebugMenu] Spawned {} Cow prefab instances ({} x {} grid, {:.2f} spacing)",
        spawnedCount,
        columns,
        rows,
        spacing);

    if (spawnedCount > 0 && m_applyResourcesCallback) {
        m_applyResourcesCallback();
    }
}

void DebugMenu::DrawPrefabDetails(const gm::scene::PrefabDefinition& prefab) {
    struct MeshAssignment {
        std::string objectName;
        std::string componentType;
        std::string meshGuid;
        std::string materialGuid;
        std::string textureGuid;
    };

    std::vector<MeshAssignment> assignments;
    std::vector<std::string> warnings;

    for (const auto& objectJson : prefab.objects) {
        if (!objectJson.is_object()) {
            continue;
        }
        const std::string objectName = objectJson.value("name", "GameObject");
        if (!objectJson.contains("components") || !objectJson["components"].is_array()) {
            continue;
        }

        for (const auto& componentJson : objectJson["components"]) {
            if (!componentJson.is_object()) {
                continue;
            }
            const std::string type = componentJson.value("type", "");
            if (type != "StaticMeshComponent" && type != "SkinnedMeshComponent") {
                continue;
            }
            const auto& data = componentJson.contains("data") && componentJson["data"].is_object()
                                   ? componentJson["data"]
                                   : nlohmann::json::object();
            MeshAssignment info;
            info.objectName = objectName;
            info.componentType = type;
            info.meshGuid = data.value("meshGuid", "");
            info.materialGuid = data.value("materialGuid", "");
            info.textureGuid = data.value("textureGuid", "");
            if (!info.meshGuid.empty() && info.materialGuid.empty()) {
                warnings.push_back(
                    fmt::format("{} '{}' references mesh '{}' without a material", type, objectName, info.meshGuid));
            }
            assignments.push_back(std::move(info));
        }
    }

    if (!warnings.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Prefab Warnings:");
        for (const auto& warning : warnings) {
            ImGui::BulletText("%s", warning.c_str());
        }
    } else {
        ImGui::TextUnformatted("Prefab Warnings: None");
    }

    if (assignments.empty()) {
        ImGui::TextDisabled("No mesh components in this prefab.");
        return;
    }

    if (ImGui::BeginTable("PrefabMeshAssignments", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Object");
        ImGui::TableSetupColumn("Component");
        ImGui::TableSetupColumn("Mesh GUID");
        ImGui::TableSetupColumn("Material GUID");
        ImGui::TableHeadersRow();

        for (const auto& info : assignments) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(info.objectName.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(info.componentType.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(info.meshGuid.empty() ? "<none>" : info.meshGuid.c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(info.materialGuid.empty() ? "<none>" : info.materialGuid.c_str());
        }

        ImGui::EndTable();
    }
}

std::filesystem::path DebugMenu::ResolveAssimpImporterExecutable() const {
    std::vector<std::filesystem::path> candidates;
    std::filesystem::path assetsDir;
    if (m_gameResources) {
        assetsDir = m_gameResources->GetAssetsDirectory();
    }
    std::filesystem::path exeName =
#if defined(_WIN32)
        "AssimpImporter.exe";
#else
        "AssimpImporter";
#endif

    auto addCandidate = [&](const std::filesystem::path& path) {
        if (!path.empty()) {
            candidates.push_back(path);
        }
    };

    auto addVsBuildCandidates = [&](const std::filesystem::path& buildDir) {
        addCandidate(buildDir / "Debug" / exeName);
        addCandidate(buildDir / "Debug" / "AssimpImporter" / exeName);
        addCandidate(buildDir / "RelWithDebInfo" / exeName);
        addCandidate(buildDir / "RelWithDebInfo" / "AssimpImporter" / exeName);
        addCandidate(buildDir / "Release" / exeName);
        addCandidate(buildDir / "Release" / "AssimpImporter" / exeName);
    };

    if (!assetsDir.empty()) {
        auto repoRoot = assetsDir.parent_path().parent_path();
        if (!repoRoot.empty()) {
            addVsBuildCandidates(repoRoot / "build");
            addCandidate(repoRoot / "build" / exeName);
            addCandidate(repoRoot / "bin" / exeName);
        }
    }

    candidates.emplace_back(exeName);

    for (const auto& candidate : candidates) {
        if (candidate.empty()) {
            continue;
        }
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec) && !ec) {
            return candidate;
        }
    }
    return {};
}

void DebugMenu::TriggerGlbReimport(const std::string& meshGuid) {
    if (!m_gameResources) {
        gm::core::Logger::Warning("[DebugMenu] GameResources unavailable; cannot re-import GLB");
        return;
    }
    if (meshGuid.empty()) {
        gm::core::Logger::Warning("[DebugMenu] Skinned mesh GUID is empty; cannot re-import");
        return;
    }

    const auto* record = m_gameResources->GetAnimsetRecordForSkinnedMesh(meshGuid);
    if (!record) {
        gm::core::Logger::Warning("[DebugMenu] No animation manifest tracked for skinned mesh '{}'", meshGuid);
        return;
    }
    if (record->sourceGlb.empty()) {
        gm::core::Logger::Warning("[DebugMenu] Animset for '{}' does not contain a GLB source path", meshGuid);
        return;
    }

    auto importer = ResolveAssimpImporterExecutable();
    if (importer.empty()) {
        gm::core::Logger::Warning("[DebugMenu] Could not locate AssimpImporter executable");
        return;
    }

    std::filesystem::path outputDir = record->outputDir;
    if (outputDir.empty()) {
        if (m_gameResources->GetAssetsDirectory().empty()) {
            gm::core::Logger::Warning("[DebugMenu] Unable to determine output directory for GLB import");
            return;
        }
        outputDir = m_gameResources->GetAssetsDirectory() / "models";
    }
    std::error_code ec;
    std::filesystem::create_directories(outputDir, ec);

    const std::string command = fmt::format("\"{}\" \"{}\" --out \"{}\" --name \"{}\"",
                                            importer.string(),
                                            record->sourceGlb,
                                            outputDir.lexically_normal().string(),
                                            record->baseName);
    gm::core::Logger::Info("[DebugMenu] Running {}", command);
    const int result = std::system(command.c_str());
    if (result != 0) {
        gm::core::Logger::Error("[DebugMenu] AssimpImporter returned exit code {}", result);
        return;
    }

    gm::core::Logger::Info("[DebugMenu] GLB re-import finished for '{}'", meshGuid);
    if (m_applyResourcesCallback) {
        m_applyResourcesCallback();
    }
}

void DebugMenu::DrawPreviewSkeleton(const ImVec2& canvasSize) {
    if (!m_previewSkeleton || m_previewBoneMatrices.empty()) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        m_previewYaw += io.MouseDelta.x * 0.01f;
        m_previewPitch += io.MouseDelta.y * 0.01f;
        const float pitchLimit = glm::radians(85.0f);
        m_previewPitch = glm::clamp(m_previewPitch, -pitchLimit, pitchLimit);
    }
    if (ImGui::IsItemHovered()) {
        const float zoomSpeed = 0.1f;
        m_previewZoom *= 1.0f - io.MouseWheel * zoomSpeed;
        m_previewZoom = glm::clamp(m_previewZoom, 0.2f, 5.0f);
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 canvasPos = ImGui::GetItemRectMin();

    std::vector<glm::vec3> positions(m_previewBoneMatrices.size());
    glm::vec3 minBounds(std::numeric_limits<float>::max());
    glm::vec3 maxBounds(std::numeric_limits<float>::lowest());
    for (std::size_t i = 0; i < m_previewBoneMatrices.size(); ++i) {
        positions[i] = glm::vec3(m_previewBoneMatrices[i][3]);
        minBounds = glm::min(minBounds, positions[i]);
        maxBounds = glm::max(maxBounds, positions[i]);
    }

    glm::vec3 center = (minBounds + maxBounds) * 0.5f;

    const glm::mat4 rotMat = glm::yawPitchRoll(m_previewYaw, m_previewPitch, 0.0f);
    const glm::mat3 rotation = glm::mat3(rotMat);

    float radius = 0.0f;
    std::vector<glm::vec3> rotated(positions.size());
    for (std::size_t i = 0; i < positions.size(); ++i) {
        glm::vec3 relative = positions[i] - center;
        rotated[i] = rotation * relative;
        radius = std::max(radius, glm::length(glm::vec2(rotated[i].x, rotated[i].y)));
    }
    if (radius < 1e-3f) {
        radius = 1.0f;
    }

    const float padding = 16.0f;
    const float size = std::min(canvasSize.x, canvasSize.y) * 0.5f - padding;
    const float scale = (size / radius) * m_previewZoom;
    const ImVec2 centerScreen = canvasPos + ImVec2(canvasSize.x * 0.5f, canvasSize.y * 0.5f);

    auto toScreen = [&](const glm::vec3& pt) -> ImVec2 {
        return centerScreen + ImVec2(pt.x * scale, -pt.y * scale);
    };

    const ImU32 lineColor = IM_COL32(0, 190, 255, 255);
    const ImU32 jointColor = IM_COL32(255, 255, 255, 255);
    const float jointRadius = 4.0f;

    for (std::size_t i = 0; i < rotated.size(); ++i) {
        const auto& bone = m_previewSkeleton->bones[i];
        if (bone.parentIndex >= 0) {
            const std::size_t parentIndex = static_cast<std::size_t>(bone.parentIndex);
            if (parentIndex < rotated.size()) {
                drawList->AddLine(toScreen(rotated[parentIndex]), toScreen(rotated[i]), lineColor, 2.0f);
            }
        }
    }

    for (std::size_t i = 0; i < rotated.size(); ++i) {
        drawList->AddCircleFilled(toScreen(rotated[i]), jointRadius, jointColor, 12);
    }

    if (ImGui::IsItemHovered()) {
        ImVec2 tooltipPos = canvasPos + ImVec2(8.0f, 8.0f);
        const char* instructions = "LMB drag: orbit  |  Mouse wheel: zoom";
        drawList->AddText(tooltipPos, IM_COL32(200, 200, 200, 220), instructions);
    }
}

void DebugMenu::HandleFileDrop(const std::vector<std::string>& paths) {
    for (const auto& path : paths) {
        std::filesystem::path filePath(path);
        std::string ext = filePath.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        if (ext == ".glb" || ext == ".gltf") {
            StartModelImport(filePath);
            break; // Only import first valid file
        }
    }
}

void DebugMenu::StartModelImport(const std::filesystem::path& filePath) {
    m_importSettings.inputPath = filePath;
    m_importSettings.baseName = filePath.stem().string();

    if (m_gameResources) {
        std::filesystem::path assetsDir = m_gameResources->GetAssetsDirectory();
        m_importSettings.outputDir = assetsDir / "models" / filePath.stem();
    } else {
        m_importSettings.outputDir = filePath.parent_path();
    }

    m_showImportDialog = true;
    m_pendingImport = false; // Already showing dialog
}

bool DebugMenu::ExecuteModelImport(const std::filesystem::path& inputPath,
                                    const std::filesystem::path& outputDir,
                                    const std::string& baseName) {
    std::filesystem::path importerExe = ResolveAssimpImporterExecutable();
    if (importerExe.empty() || !std::filesystem::exists(importerExe)) {
        gm::core::Logger::Error("[DebugMenu] AssimpImporter executable not found");
        m_importStatusMessage = "Error: AssimpImporter executable not found";
        return false;
    }

    // Create output directory if it doesn't exist
    std::error_code ec;
    std::filesystem::create_directories(outputDir, ec);
    if (ec) {
        gm::core::Logger::Error("[DebugMenu] Failed to create output directory: {}", ec.message());
        m_importStatusMessage = fmt::format("Error: Failed to create output directory: {}", ec.message());
        return false;
    }

    // Build command line
    std::string cmd = fmt::format("\"{}\" \"{}\" --out \"{}\" --name \"{}\"",
                                  importerExe.string(),
                                  inputPath.string(),
                                  outputDir.string(),
                                  baseName);

    gm::core::Logger::Info("[DebugMenu] Executing import: {}", cmd);

#ifdef _WIN32
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi = {};
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    std::vector<char> cmdLine(cmd.begin(), cmd.end());
    cmdLine.push_back('\0');

    BOOL success = CreateProcessA(
        nullptr,
        cmdLine.data(),
        nullptr,
        nullptr,
        TRUE,
        0,
        nullptr,
        nullptr,
        &si,
        &pi);

    if (!success) {
        DWORD error = GetLastError();
        gm::core::Logger::Error("[DebugMenu] Failed to start import process: {}", error);
        m_importStatusMessage = fmt::format("Error: Failed to start import process (code {})", error);
        return false;
    }

    // Wait for process to complete
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exitCode != 0) {
        gm::core::Logger::Error("[DebugMenu] Import process exited with code {}", exitCode);
        m_importStatusMessage = fmt::format("Error: Import failed (exit code {})", exitCode);
        return false;
    }
#else
    int result = std::system(cmd.c_str());
    if (result != 0) {
        gm::core::Logger::Error("[DebugMenu] Import process exited with code {}", result);
        m_importStatusMessage = fmt::format("Error: Import failed (exit code {})", result);
        return false;
    }
#endif

    gm::core::Logger::Info("[DebugMenu] Model import completed successfully");
    return true;
}

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS
