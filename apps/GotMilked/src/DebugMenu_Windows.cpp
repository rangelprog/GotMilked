#if GM_DEBUG_TOOLS

#include "DebugMenu.hpp"
#include "EditableTerrainComponent.hpp"
#include "GameConstants.hpp"
#include "gm/scene/PrefabLibrary.hpp"

#include "gm/scene/Scene.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/StaticMeshComponent.hpp"
#include "gm/scene/LightComponent.hpp"
#include "gm/physics/RigidBodyComponent.hpp"
#include "gm/core/Logger.hpp"

#include <cstdint>

#include <imgui.h>
#include <imgui_internal.h>
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <ImGuizmo.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <unordered_map>

namespace {
ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) {
    return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y);
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

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const glm::mat4 viewProj = proj * view;

    auto selected = m_selectedGameObject.lock();

    const float circleRadius = 6.0f;
    const float hoverRadius = circleRadius * 1.8f;
    const ImU32 defaultColor = IM_COL32(255, 255, 255, 255);
    const ImU32 selectedColor = IM_COL32(255, 220, 0, 255);

    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 mousePos = io.MousePos;

    for (const auto& gameObject : scene->GetAllGameObjects()) {
        if (!gameObject || !gameObject->IsActive() || gameObject->IsDestroyed()) {
            continue;
        }

        auto transform = gameObject->GetTransform();
        if (!transform) {
            continue;
        }

        glm::vec3 worldPos = transform->GetPosition();
        glm::vec4 clipPos = viewProj * glm::vec4(worldPos, 1.0f);

        if (std::abs(clipPos.w) < 1e-6f) {
            continue;
        }

        glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;

        if (ndc.z < -1.0f || ndc.z > 1.0f ||
            ndc.x < -1.0f || ndc.x > 1.0f ||
            ndc.y < -1.0f || ndc.y > 1.0f) {
            continue;
        }

        float screenX = (ndc.x + gotmilked::GameConstants::Rendering::NdcOffset) *
                        gotmilked::GameConstants::Rendering::NdcToScreenScale *
                        static_cast<float>(viewportWidth);
        float screenY = (gotmilked::GameConstants::Rendering::NdcOffset - ndc.y) *
                        gotmilked::GameConstants::Rendering::NdcToScreenScale *
                        static_cast<float>(viewportHeight);

        ImVec2 screenPos(viewport->Pos.x + screenX, viewport->Pos.y + screenY);

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

    ImGui::End();
    ImGui::PopStyleVar(3);
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

    auto allObjects = scene->GetAllGameObjects();
    std::string filterStr(searchFilter);
    std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(),
                   [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });

    int visibleCount = 0;
    int loopIndex = 0;
    for (const auto& gameObject : allObjects) {
        if (!gameObject || gameObject->IsDestroyed()) {
            continue;
        }

        std::string name = gameObject->GetName();
        if (name.empty()) {
            name = "Unnamed GameObject";
        }

        if (!filterStr.empty()) {
            std::string lowerName = name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                           [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
            if (lowerName.find(filterStr) == std::string::npos) {
                continue;
            }
        }

        visibleCount++;

        auto selected = m_selectedGameObject.lock();
        bool isSelected = (selected && selected == gameObject);

        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
        }

        std::string displayName = name;
        if (!gameObject->IsActive()) {
            displayName += " [Inactive]";
        }

        ImGui::PushID(gameObject.get());
        bool itemActivated = ImGui::Selectable(displayName.c_str(), isSelected);
        if (itemActivated) {
            m_selectedGameObject = gameObject;
            EnsureSelectionWindowsVisible();
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            EnsureSelectionWindowsVisible();
            FocusCameraOnGameObject(gameObject);
        }
        ImGui::PopID();

        if (isSelected) {
            ImGui::PopStyleColor();
        }

        ++loopIndex;
    }

    if (visibleCount == 0 && !filterStr.empty()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No GameObjects match filter");
    } else if (visibleCount == 0) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No GameObjects in scene");
    }
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

            ImGui::Separator();
            const char* opItems[] = { "Translate", "Rotate", "Scale" };
            ImGui::Combo("Gizmo Operation", &m_gizmoOperation, opItems, IM_ARRAYSIZE(opItems));
            const char* modeItems[] = { "World", "Local" };
            ImGui::Combo("Gizmo Mode", &m_gizmoMode, modeItems, IM_ARRAYSIZE(modeItems));
            ImGui::TextUnformatted("Hotkeys: W (Translate), E (Rotate), R (Scale)");
        }
    }

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
                ImGui::TextUnformatted("Hold LMB to raise terrain.");
                ImGui::TextUnformatted("Hold RMB to lower terrain.");
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
                }
            }
        }
    }

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

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS
