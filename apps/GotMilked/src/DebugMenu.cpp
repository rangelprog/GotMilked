#include "DebugMenu.hpp"

#include "gm/scene/Scene.hpp"
#include "gm/scene/SceneSerializer.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/save/SaveManager.hpp"
#include "gm/core/Logger.hpp"
#include "EditableTerrainComponent.hpp"
#include <imgui.h>
#include <filesystem>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

void DebugMenu::Render(bool& menuVisible) {
    if (!menuVisible) {
        return;
    }

    if (ImGui::BeginMainMenuBar()) {
        RenderMenuBar();
        ImGui::EndMainMenuBar();
    }

    if (m_showSaveAsDialog) {
        RenderSaveAsDialog();
    }

    if (m_showLoadDialog) {
        RenderLoadDialog();
    }

    // Render GameObject labels if enabled
    if (m_showGameObjects) {
        RenderGameObjectLabels();
    }
}

void DebugMenu::RenderMenuBar() {
    if (ImGui::BeginMenu("File")) {
        m_fileMenuOpen = true;

        if (ImGui::MenuItem("Quick Save", "F5")) {
            if (m_callbacks.quickSave) {
                m_callbacks.quickSave();
            }
        }

        if (ImGui::MenuItem("Quick Load", "F9")) {
            if (m_callbacks.quickLoad) {
                m_callbacks.quickLoad();
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Save Scene As...")) {
#ifdef _WIN32
            OPENFILENAMEA ofn;
            char szFile[260] = {0};
            
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = static_cast<HWND>(m_windowHandle);
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = "JSON Files\0*.json\0All Files\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrFileTitle = nullptr;
            ofn.nMaxFileTitle = 0;
            std::string initialDir = m_defaultScenePath;
            if (!initialDir.empty() && std::filesystem::exists(initialDir)) {
                ofn.lpstrInitialDir = initialDir.c_str();
            }
            ofn.Flags = OFN_PATHMUSTEXIST;
            ofn.lpstrDefExt = "json";
            
            if (GetSaveFileNameA(&ofn)) {
                std::string filePath(szFile);
                auto scene = m_scene.lock();
                if (scene) {
                    // Check if file exists and generate unique name if needed
                    std::filesystem::path path(filePath);
                    if (std::filesystem::exists(path)) {
                        // Generate unique filename by appending number
                        std::filesystem::path dir = path.parent_path();
                        std::string stem = path.stem().string();
                        std::string ext = path.extension().string();
                        
                        int counter = 1;
                        std::filesystem::path newPath;
                        do {
                            std::string newName = stem + "_" + std::to_string(counter) + ext;
                            newPath = dir / newName;
                            counter++;
                        } while (std::filesystem::exists(newPath) && counter < 1000);
                        
                        if (counter < 1000) {
                            filePath = newPath.string();
                            gm::core::Logger::Info("[DebugMenu] File exists, saving as: %s", filePath.c_str());
                        } else {
                            gm::core::Logger::Error("[DebugMenu] Too many duplicate files, cannot generate unique name");
                            return; // Skip saving if we can't generate a unique name
                        }
                    }
                    
                    std::filesystem::path finalPath(filePath);
                    std::filesystem::path dir = finalPath.parent_path();
                    if (!dir.empty()) {
                        std::error_code ec;
                        std::filesystem::create_directories(dir, ec);
                    }
                    
                    // Log terrain component before saving
                    auto terrainObject = scene->FindGameObjectByName("Terrain");
                    if (terrainObject) {
                        if (auto terrain = terrainObject->GetComponent<EditableTerrainComponent>()) {
                            gm::core::Logger::Info("[DebugMenu] Terrain component found before save, will be serialized");
                        } else {
                            gm::core::Logger::Warning("[DebugMenu] Terrain component NOT found before save!");
                        }
                    }
                    
                    // Get scene JSON and add camera data
                    std::string sceneJsonStr = gm::SceneSerializer::Serialize(*scene);
                    nlohmann::json sceneJson = nlohmann::json::parse(sceneJsonStr);
                    
                    // Add camera data if callbacks are available
                    if (m_callbacks.getCameraPosition && m_callbacks.getCameraForward && m_callbacks.getCameraFov) {
                        glm::vec3 pos = m_callbacks.getCameraPosition();
                        glm::vec3 forward = m_callbacks.getCameraForward();
                        float fov = m_callbacks.getCameraFov();
                        
                        sceneJson["camera"] = {
                            {"position", {pos.x, pos.y, pos.z}},
                            {"forward", {forward.x, forward.y, forward.z}},
                            {"fov", fov}
                        };
                        gm::core::Logger::Info("[DebugMenu] Added camera data to scene: pos=(%.2f, %.2f, %.2f)", 
                            pos.x, pos.y, pos.z);
                    }
                    
                    // Write the modified JSON to file
                    std::ofstream file(filePath);
                    if (file.is_open()) {
                        file << sceneJson.dump(4);
                        file.close();
                        gm::core::Logger::Info("[DebugMenu] Scene saved to: %s", filePath.c_str());
                    } else {
                        gm::core::Logger::Error("[DebugMenu] Failed to open file for writing: %s", filePath.c_str());
                    }
                }
            }
#else
            m_showSaveAsDialog = true;
            std::memset(m_filePathBuffer, 0, sizeof(m_filePathBuffer));
            std::string defaultPath = m_defaultScenePath + "scene.json";
            std::size_t copyLen = std::min(defaultPath.length(), sizeof(m_filePathBuffer) - 1);
            std::memcpy(m_filePathBuffer, defaultPath.c_str(), copyLen);
            m_filePathBuffer[copyLen] = '\0';
#endif
        }

        if (ImGui::MenuItem("Load Scene From...")) {
#ifdef _WIN32
            OPENFILENAMEA ofn;
            char szFile[260] = {0};
            
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = static_cast<HWND>(m_windowHandle);
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = "JSON Files\0*.json\0All Files\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrFileTitle = nullptr;
            ofn.nMaxFileTitle = 0;
            std::string initialDir = m_defaultScenePath;
            if (!initialDir.empty() && std::filesystem::exists(initialDir)) {
                ofn.lpstrInitialDir = initialDir.c_str();
            }
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
            
            if (GetOpenFileNameA(&ofn)) {
                std::string filePath(szFile);
                auto scene = m_scene.lock();
                if (scene) {
                    // Read the JSON file
                    std::ifstream file(filePath);
                    if (!file.is_open()) {
                        gm::core::Logger::Error("[DebugMenu] Failed to open file for reading: %s", filePath.c_str());
                        return;
                    }
                    
                    std::stringstream buffer;
                    buffer << file.rdbuf();
                    file.close();
                    
                    nlohmann::json sceneJson = nlohmann::json::parse(buffer.str());
                    
                    // Extract camera data before deserializing scene
                    if (sceneJson.contains("camera") && m_callbacks.setCamera) {
                        const auto& cameraJson = sceneJson["camera"];
                        if (cameraJson.contains("position") && cameraJson.contains("forward")) {
                            auto pos = cameraJson["position"];
                            auto forward = cameraJson["forward"];
                            float fov = cameraJson.value("fov", 60.0f);
                            
                            if (pos.is_array() && pos.size() == 3 && 
                                forward.is_array() && forward.size() == 3) {
                                glm::vec3 position(pos[0].get<float>(), pos[1].get<float>(), pos[2].get<float>());
                                glm::vec3 forwardVec(forward[0].get<float>(), forward[1].get<float>(), forward[2].get<float>());
                                
                                m_callbacks.setCamera(position, forwardVec, fov);
                                gm::core::Logger::Info("[DebugMenu] Restored camera: pos=(%.2f, %.2f, %.2f), fov=%.2f", 
                                    position.x, position.y, position.z, fov);
                            }
                        }
                    }
                    
                    // Deserialize scene (this will remove camera data from JSON)
                    std::string sceneJsonStr = sceneJson.dump();
                    if (gm::SceneSerializer::Deserialize(*scene, sceneJsonStr)) {
                        gm::core::Logger::Info("[DebugMenu] Scene loaded from: %s", filePath.c_str());
                        scene->Init();
                        if (m_callbacks.onSceneLoaded) {
                            m_callbacks.onSceneLoaded();
                        }
                    } else {
                        gm::core::Logger::Error("[DebugMenu] Failed to load scene from: %s", filePath.c_str());
                    }
                }
            }
#else
            m_showLoadDialog = true;
            std::memset(m_filePathBuffer, 0, sizeof(m_filePathBuffer));
#endif
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Reload Resources")) {
            if (m_callbacks.reloadResources) {
                m_callbacks.reloadResources();
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Close Menu", "F1")) {
            // Menu visibility will be toggled by the caller
        }

        ImGui::EndMenu();
    } else {
        m_fileMenuOpen = false;
    }

    if (ImGui::BeginMenu("Edit")) {
        m_editMenuOpen = true;

        if (m_terrainComponent) {
            bool terrainEditorVisible = m_terrainComponent->IsEditorWindowVisible();
            if (ImGui::MenuItem("Terrain Editor", nullptr, terrainEditorVisible)) {
                m_terrainComponent->SetEditorWindowVisible(!terrainEditorVisible);
            }
        } else {
            ImGui::MenuItem("Terrain Editor", nullptr, false, false);
        }

        ImGui::Separator();

        // Placeholder for edit menu items
        ImGui::MenuItem("Undo", nullptr, false, false);
        ImGui::MenuItem("Redo", nullptr, false, false);

        ImGui::Separator();

        ImGui::MenuItem("Preferences...", nullptr, false, false);

        ImGui::EndMenu();
    } else {
        m_editMenuOpen = false;
    }

    if (ImGui::BeginMenu("Options")) {
        m_optionsMenuOpen = true;

        ImGui::Checkbox("Show GameObjects", &m_showGameObjects);

        ImGui::EndMenu();
    } else {
        m_optionsMenuOpen = false;
    }
}

void DebugMenu::RenderFileMenu() {
    // This is called from RenderMenuBar via BeginMenu
}

void DebugMenu::RenderEditMenu() {
    // This is called from RenderMenuBar via BeginMenu
}

void DebugMenu::RenderOptionsMenu() {
    // This is called from RenderMenuBar via BeginMenu
}

void DebugMenu::RenderGameObjectLabels() {
    auto scene = m_scene.lock();
    if (!scene) {
        return;
    }

    // Check if callbacks are valid (std::function can be checked by converting to bool)
    if (!static_cast<bool>(m_callbacks.getViewMatrix) || 
        !static_cast<bool>(m_callbacks.getProjectionMatrix) || 
        !static_cast<bool>(m_callbacks.getViewportSize)) {
        return;
    }

    glm::mat4 view = m_callbacks.getViewMatrix();
    glm::mat4 proj = m_callbacks.getProjectionMatrix();
    int viewportWidth = 0, viewportHeight = 0;
    m_callbacks.getViewportSize(viewportWidth, viewportHeight);

    if (viewportWidth <= 0 || viewportHeight <= 0) {
        return;
    }

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) {
        return;
    }

    const glm::mat4 viewProj = proj * view;
    const float dotSize = 8.0f;
    const ImU32 dotColor = IM_COL32(255, 255, 255, 255);
    const ImU32 textColor = IM_COL32(255, 255, 255, 255);

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

        // Perspective divide
        if (std::abs(clipPos.w) < 1e-6f) {
            continue;
        }

        glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;

        // Check if object is behind camera or outside view frustum
        if (ndc.z < -1.0f || ndc.z > 1.0f || 
            ndc.x < -1.0f || ndc.x > 1.0f || 
            ndc.y < -1.0f || ndc.y > 1.0f) {
            continue;
        }

        // Convert NDC to screen coordinates
        float screenX = (ndc.x + 1.0f) * 0.5f * static_cast<float>(viewportWidth);
        float screenY = (1.0f - ndc.y) * 0.5f * static_cast<float>(viewportHeight);

        ImVec2 screenPos(screenX, screenY);

        // Draw white dot
        drawList->AddCircleFilled(screenPos, dotSize, dotColor, 16);

        // Draw name above the dot
        const std::string& name = gameObject->GetName();
        if (!name.empty()) {
            ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
            ImVec2 textPos(screenPos.x - textSize.x * 0.5f, screenPos.y - dotSize - textSize.y - 4.0f);
            
            // Draw text with background for better visibility
            ImVec2 textMin = textPos;
            ImVec2 textMax(textPos.x + textSize.x, textPos.y + textSize.y);
            drawList->AddRectFilled(
                ImVec2(textMin.x - 2.0f, textMin.y - 2.0f),
                ImVec2(textMax.x + 2.0f, textMax.y + 2.0f),
                IM_COL32(0, 0, 0, 128), 2.0f);
            
            drawList->AddText(textPos, textColor, name.c_str());
        }
    }
}

void DebugMenu::RenderSaveAsDialog() {
    if (!ImGui::Begin("Save Scene As", &m_showSaveAsDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    ImGui::Text("Enter file path:");
    ImGui::InputText("##FilePath", m_filePathBuffer, sizeof(m_filePathBuffer));

    auto scene = m_scene.lock();
    if (!scene) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: No scene available");
        ImGui::End();
        return;
    }

    ImGui::Separator();

    if (ImGui::Button("Save", ImVec2(120, 0))) {
        std::string filePath(m_filePathBuffer);
        if (!filePath.empty()) {
            // Ensure directory exists
            std::filesystem::path path(filePath);
            std::filesystem::path dir = path.parent_path();
            if (!dir.empty()) {
                std::error_code ec;
                std::filesystem::create_directories(dir, ec);
            }

            if (gm::SceneSerializer::SaveToFile(*scene, filePath)) {
                gm::core::Logger::Info("[DebugMenu] Scene saved to: %s", filePath.c_str());
                m_showSaveAsDialog = false;
            } else {
                gm::core::Logger::Error("[DebugMenu] Failed to save scene to: %s", filePath.c_str());
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        m_showSaveAsDialog = false;
    }

    ImGui::End();
}

void DebugMenu::RenderLoadDialog() {
    if (!ImGui::Begin("Load Scene From", &m_showLoadDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    ImGui::Text("Enter file path:");
    ImGui::InputText("##FilePath", m_filePathBuffer, sizeof(m_filePathBuffer));

    auto scene = m_scene.lock();
    if (!scene) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: No scene available");
        ImGui::End();
        return;
    }

    ImGui::Separator();

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Warning: This will replace the current scene!");

    ImGui::Separator();

    if (ImGui::Button("Load", ImVec2(120, 0))) {
        std::string filePath(m_filePathBuffer);
        if (!filePath.empty()) {
            if (std::filesystem::exists(filePath)) {
                if (gm::SceneSerializer::LoadFromFile(*scene, filePath)) {
                    gm::core::Logger::Info("[DebugMenu] Scene loaded from: %s", filePath.c_str());
                    scene->Init();
                    if (m_callbacks.onSceneLoaded) {
                        m_callbacks.onSceneLoaded();
                    }
                    m_showLoadDialog = false;
                } else {
                    gm::core::Logger::Error("[DebugMenu] Failed to load scene from: %s", filePath.c_str());
                }
            } else {
                gm::core::Logger::Error("[DebugMenu] File not found: %s", filePath.c_str());
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        m_showLoadDialog = false;
    }

    ImGui::End();
}

