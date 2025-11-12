#ifdef _DEBUG
#include "gm/tooling/DebugConsole.hpp"
#endif
#include "DebugMenu.hpp"
#include "GameConstants.hpp"

#include "gm/scene/Scene.hpp"
#include "gm/scene/SceneSerializer.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/StaticMeshComponent.hpp"
#include "gm/scene/LightComponent.hpp"
#include "gm/physics/RigidBodyComponent.hpp"
#include "gm/save/SaveManager.hpp"
#include "gm/save/SaveSnapshotHelpers.hpp"
#include "gm/rendering/Camera.hpp"
#include "gm/core/Logger.hpp"
#include "EditableTerrainComponent.hpp"
#include "gm/utils/FileDialog.hpp"
#include <imgui.h>
#include <filesystem>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>

void DebugMenu::Render(bool& menuVisible) {
    if (!menuVisible) {
        return;
    }

    if (ImGui::BeginMainMenuBar()) {
        RenderMenuBar();
        ImGui::EndMainMenuBar();
    }

    // Handle deferred file dialogs (after menu bar is closed)
    if (m_pendingSaveAs) {
        m_pendingSaveAs = false;
        HandleSaveAs();
    }

    if (m_pendingLoad) {
        m_pendingLoad = false;
        HandleLoad();
    }

    if (m_showSaveAsDialog) {
        RenderSaveAsDialog();
    }

    if (m_showLoadDialog) {
        RenderLoadDialog();
    }

    // Render editor windows
    if (m_showInspector) {
        RenderEditorWindow();
    }
    if (m_showSceneInfo) {
        RenderSceneInfo();
    }

    // Render GameObject labels if enabled
    if (m_showGameObjects) {
        RenderGameObjectLabels();
    }

#ifdef _DEBUG
    if (m_showDebugConsole && m_debugConsole) {
        bool open = m_showDebugConsole;
        m_debugConsole->Render(&open);
        m_showDebugConsole = open;
    }
#endif

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

        if (ImGui::MenuItem("Save Scene As...", "Ctrl+S")) {
            // Defer file dialog to avoid ImGui state issues
            m_pendingSaveAs = true;
        }

        if (ImGui::MenuItem("Load Scene From...", "Ctrl+O")) {
            // Defer file dialog to avoid ImGui state issues
            m_pendingLoad = true;
        }

        // Recent Files submenu
        if (!m_recentFiles.empty()) {
            ImGui::Separator();
            if (ImGui::BeginMenu("Recent Files")) {
                for (size_t i = 0; i < m_recentFiles.size(); ++i) {
                    const std::string& filePath = m_recentFiles[i];
                    std::filesystem::path path(filePath);
                    std::string displayName = path.filename().string();
                    if (displayName.empty()) {
                        displayName = filePath;
                    }
                    
                    if (ImGui::MenuItem(displayName.c_str())) {
                        LoadRecentFile(filePath);
                    }
                }
                ImGui::EndMenu();
            }
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

        // Placeholder for edit menu items
        ImGui::MenuItem("Undo", nullptr, false, false);
        ImGui::MenuItem("Redo", nullptr, false, false);

        ImGui::Separator();

        ImGui::MenuItem("Preferences...", nullptr, false, false);

        ImGui::EndMenu();
    } else {
        m_editMenuOpen = false;
    }

    if (ImGui::BeginMenu("View")) {
        m_optionsMenuOpen = true;

        ImGui::Checkbox("Scene Editor", &m_showInspector);
        ImGui::Checkbox("Scene Info", &m_showSceneInfo);
        ImGui::Separator();
        ImGui::Checkbox("Show GameObjects", &m_showGameObjects);

        ImGui::EndMenu();
    } else {
        m_optionsMenuOpen = false;
    }

#ifdef _DEBUG
    if (ImGui::BeginMenu("Debug")) {
        ImGui::MenuItem("Console", nullptr, &m_showDebugConsole);
        ImGui::EndMenu();
    }
#endif
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
        float screenX = (ndc.x + gotmilked::GameConstants::Rendering::NdcOffset) * 
                        gotmilked::GameConstants::Rendering::NdcToScreenScale * 
                        static_cast<float>(viewportWidth);
        float screenY = (gotmilked::GameConstants::Rendering::NdcOffset - ndc.y) * 
                        gotmilked::GameConstants::Rendering::NdcToScreenScale * 
                        static_cast<float>(viewportHeight);

        ImVec2 screenPos(screenX, screenY);

        // Draw white dot
        drawList->AddCircleFilled(screenPos, gotmilked::GameConstants::Rendering::DotSize, dotColor, 16);

        // Draw name above the dot
        const std::string& name = gameObject->GetName();
        if (!name.empty()) {
            ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
            ImVec2 textPos(
                screenPos.x - textSize.x * gotmilked::GameConstants::Rendering::LabelTextOffset, 
                screenPos.y - gotmilked::GameConstants::Rendering::DotSize - textSize.y - gotmilked::GameConstants::Rendering::LabelOffsetY);
            
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
                if (ec) {
                    gm::core::Logger::Error("[DebugMenu] Failed to create directory {}: {}", 
                                dir.string(), ec.message());
                }
            }

            if (gm::SceneSerializer::SaveToFile(*scene, filePath)) {
                gm::core::Logger::Info("[DebugMenu] Scene saved to: {}", filePath);
                m_showSaveAsDialog = false;
                AddRecentFile(filePath);
            } else {
                gm::core::Logger::Error("[DebugMenu] Failed to save scene to: {}", filePath);
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
                    gm::core::Logger::Info("[DebugMenu] Scene loaded from: {}", filePath);
                    scene->Init();
                    if (m_callbacks.onSceneLoaded) {
                        m_callbacks.onSceneLoaded();
                    }
                    m_showLoadDialog = false;
                    AddRecentFile(filePath);
                } else {
                    gm::core::Logger::Error("[DebugMenu] Failed to load scene from: {}", filePath);
                }
            } else {
                gm::core::Logger::Error("[DebugMenu] File not found: {}", filePath);
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        m_showLoadDialog = false;
    }

    ImGui::End();
}

void DebugMenu::HandleSaveAs() {
    auto result = gm::utils::FileDialog::SaveFile(
        "JSON Files\0*.json\0All Files\0*.*\0",
        "json",
        m_defaultScenePath,
        m_windowHandle);
    
    if (result.has_value()) {
        std::string filePath = result.value();
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
                    gm::core::Logger::Info("[DebugMenu] File exists, saving as: {}", filePath);
                } else {
                    gm::core::Logger::Error("[DebugMenu] Too many duplicate files, cannot generate unique name");
                    return;
                }
            }
            
            std::filesystem::path finalPath(filePath);
            std::filesystem::path dir = finalPath.parent_path();
            if (!dir.empty()) {
                std::error_code ec;
                std::filesystem::create_directories(dir, ec);
                // Only report error if directory doesn't exist AND creation failed
                if (ec && !std::filesystem::exists(dir, ec)) {
                    gm::core::Logger::Error("[DebugMenu] Failed to create directory: {}", ec.message());
                    return;
                }
            }
            
            // Capture save data exactly like Quick Save does
            gm::save::SaveGameData data;
            
            // Set scene name
            data.sceneName = scene->GetName();
            
            // Capture camera data from callbacks (same as Quick Save)
            if (m_callbacks.getCameraPosition && m_callbacks.getCameraForward && m_callbacks.getCameraFov) {
                data.cameraPosition = m_callbacks.getCameraPosition();
                data.cameraForward = m_callbacks.getCameraForward();
                data.cameraFov = m_callbacks.getCameraFov();
            }
            
            // Capture world time (same as Quick Save)
            if (m_callbacks.getWorldTime) {
                data.worldTime = m_callbacks.getWorldTime();
            }
            
            // Serialize the scene using SceneSerializer to save all GameObjects and their properties
            std::string sceneJsonString = gm::SceneSerializer::Serialize(*scene);
            nlohmann::json sceneJson = nlohmann::json::parse(sceneJsonString);
            
            // Add camera and world time data to the scene JSON (preserve Quick Save compatibility)
            if (m_callbacks.getCameraPosition && m_callbacks.getCameraForward && m_callbacks.getCameraFov) {
                sceneJson["camera"] = {
                    {"position", {data.cameraPosition.x, data.cameraPosition.y, data.cameraPosition.z}},
                    {"forward", {data.cameraForward.x, data.cameraForward.y, data.cameraForward.z}},
                    {"fov", data.cameraFov}
                };
            }
            
            if (m_callbacks.getWorldTime) {
                sceneJson["worldTime"] = data.worldTime;
            }
            
            // Add version and sceneName for compatibility
            sceneJson["version"] = data.version;
            sceneJson["sceneName"] = data.sceneName;
            
            // Write to file with indentation 2 (same as Quick Save format)
            std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
            if (!file.is_open()) {
                gm::core::Logger::Error("[DebugMenu] Failed to open file for writing: {}", filePath);
                return;
            }
            
            file << sceneJson.dump(2);
            if (!file.good()) {
                gm::core::Logger::Error("[DebugMenu] Failed to write to file: {}", filePath);
                file.close();
                return;
            }
            
            file.close();
            gm::core::Logger::Info("[DebugMenu] Scene saved to: {} (includes all GameObjects and properties)", filePath);
            AddRecentFile(filePath);
        }
    } else {
        // File dialog was cancelled or failed - show fallback dialog on non-Windows
#ifndef _WIN32
        m_showSaveAsDialog = true;
        std::memset(m_filePathBuffer, 0, sizeof(m_filePathBuffer));
        std::string defaultPath = m_defaultScenePath + "scene.json";
        std::size_t copyLen = std::min(defaultPath.length(), sizeof(m_filePathBuffer) - 1);
        std::memcpy(m_filePathBuffer, defaultPath.c_str(), copyLen);
        m_filePathBuffer[copyLen] = '\0';
#endif
    }
}

void DebugMenu::HandleLoad() {
    auto result = gm::utils::FileDialog::OpenFile(
        "JSON Files\0*.json\0All Files\0*.*\0",
        m_defaultScenePath,
        m_windowHandle);
    
    if (result.has_value()) {
        std::string filePath = result.value();
        auto scene = m_scene.lock();
        if (scene) {
            // Read the JSON file
            std::ifstream file(filePath);
            if (!file.is_open()) {
                gm::core::Logger::Error("[DebugMenu] Failed to open file for reading: {}", filePath);
                return;
            }
            
            std::stringstream buffer;
            buffer << file.rdbuf();
            file.close();
            
            nlohmann::json sceneJson;
            try {
                sceneJson = nlohmann::json::parse(buffer.str());
            } catch (const std::exception& ex) {
                gm::core::Logger::Error("[DebugMenu] Failed to parse JSON file {}: {}", filePath, ex.what());
                return;
            }
            
            // Check if this is a scene file with GameObjects (new format or old scene format)
            if (sceneJson.contains("gameObjects") && sceneJson["gameObjects"].is_array()) {
                // This is a scene file - deserialize using SceneSerializer
                if (!gm::SceneSerializer::Deserialize(*scene, buffer.str())) {
                    gm::core::Logger::Error("[DebugMenu] Failed to deserialize scene from: {}", filePath);
                    return;
                }
                
                // Also apply camera and world time if present (combined format)
                if (sceneJson.contains("camera")) {
                    const auto& camera = sceneJson["camera"];
                    if (camera.contains("position") && camera.contains("forward") && camera.contains("fov")) {
                        auto pos = camera["position"];
                        auto fwd = camera["forward"];
                        if (pos.is_array() && pos.size() == 3 && fwd.is_array() && fwd.size() == 3) {
                            glm::vec3 cameraPos(pos[0].get<float>(), pos[1].get<float>(), pos[2].get<float>());
                            glm::vec3 cameraFwd(fwd[0].get<float>(), fwd[1].get<float>(), fwd[2].get<float>());
                            float cameraFov = camera.value("fov", 60.0f);
                            if (m_callbacks.setCamera) {
                                m_callbacks.setCamera(cameraPos, cameraFwd, cameraFov);
                            }
                        }
                    }
                }
                
                // Note: worldTime is saved but not restored (no setWorldTime callback)
                // World time is managed by the game itself
                
                gm::core::Logger::Info("[DebugMenu] Scene loaded from: {} (with GameObjects)", filePath);
                AddRecentFile(filePath);
                if (m_callbacks.onSceneLoaded) {
                    m_callbacks.onSceneLoaded();
                }
                return;
            }
            
            // Check if this is a Quick Save format file (has version, sceneName, camera, worldTime)
            // but no gameObjects (old Quick Save format)
            if (sceneJson.contains("version") && sceneJson.contains("sceneName") && 
                sceneJson.contains("camera") && sceneJson.contains("worldTime")) {
                // This is a Quick Save format file - parse it manually
                gm::save::SaveGameData data;
                try {
                    data.version = sceneJson.value("version", data.version);
                    data.sceneName = sceneJson.value("sceneName", data.sceneName);
                    
                    if (sceneJson.contains("camera")) {
                        const auto& camera = sceneJson["camera"];
                        if (camera.contains("position")) {
                            auto pos = camera["position"];
                            if (pos.is_array() && pos.size() == 3) {
                                data.cameraPosition = glm::vec3(
                                    pos[0].get<float>(),
                                    pos[1].get<float>(),
                                    pos[2].get<float>());
                            }
                        }
                        if (camera.contains("forward")) {
                            auto fwd = camera["forward"];
                            if (fwd.is_array() && fwd.size() == 3) {
                                data.cameraForward = glm::vec3(
                                    fwd[0].get<float>(),
                                    fwd[1].get<float>(),
                                    fwd[2].get<float>());
                            }
                        }
                        data.cameraFov = camera.value("fov", data.cameraFov);
                    }
                    
                    data.worldTime = sceneJson.value("worldTime", data.worldTime);
                    
                    if (sceneJson.contains("terrain")) {
                        const auto& terrain = sceneJson["terrain"];
                        data.terrainResolution = terrain.value("resolution", data.terrainResolution);
                        data.terrainSize = terrain.value("size", data.terrainSize);
                        data.terrainMinHeight = terrain.value("minHeight", data.terrainMinHeight);
                        data.terrainMaxHeight = terrain.value("maxHeight", data.terrainMaxHeight);
                        
                        if (terrain.contains("heights") && terrain["heights"].is_array()) {
                            const auto& heights = terrain["heights"];
                            data.terrainHeights.clear();
                            data.terrainHeights.reserve(heights.size());
                            for (const auto& value : heights) {
                                data.terrainHeights.push_back(value.get<float>());
                            }
                        }
                    }
                    
                    // Apply camera
                    if (m_callbacks.setCamera) {
                        m_callbacks.setCamera(data.cameraPosition, data.cameraForward, data.cameraFov);
                    }
                    
                    // Apply terrain if present
                    if (data.terrainResolution > 0 && !data.terrainHeights.empty()) {
                        auto terrainObject = scene->FindGameObjectByName("Terrain");
                        if (terrainObject) {
                            if (auto terrain = terrainObject->GetComponent<EditableTerrainComponent>()) {
                                bool ok = terrain->SetHeightData(
                                    data.terrainResolution,
                                    data.terrainSize,
                                    data.terrainMinHeight,
                                    data.terrainMaxHeight,
                                    data.terrainHeights);
                                if (ok) {
                                    terrain->MarkMeshDirty();
                                    gm::core::Logger::Info("[DebugMenu] Terrain loaded from save file");
                                }
                            }
                        }
                    }
                    
                    gm::core::Logger::Info("[DebugMenu] Quick Save format file loaded from: {}", filePath);
                    if (m_callbacks.onSceneLoaded) {
                        m_callbacks.onSceneLoaded();
                    }
                } catch (const std::exception& ex) {
                    gm::core::Logger::Error("[DebugMenu] Failed to parse Quick Save format: {}", ex.what());
                }
            } else {
                // This is a scene file - use SceneSerializer
                // Extract camera data before deserializing scene
                if (sceneJson.contains("camera") && m_callbacks.setCamera) {
                    const auto& cameraJson = sceneJson["camera"];
                    if (cameraJson.contains("position") && cameraJson.contains("forward")) {
                        auto pos = cameraJson["position"];
                        auto forward = cameraJson["forward"];
                        float fov = cameraJson.value("fov", gotmilked::GameConstants::Camera::DefaultFovDegrees);
                        
                        if (pos.is_array() && pos.size() == 3 && 
                            forward.is_array() && forward.size() == 3) {
                            glm::vec3 position(pos[0].get<float>(), pos[1].get<float>(), pos[2].get<float>());
                            glm::vec3 forwardVec(forward[0].get<float>(), forward[1].get<float>(), forward[2].get<float>());
                            
                            m_callbacks.setCamera(position, forwardVec, fov);
                            gm::core::Logger::Info("[DebugMenu] Restored camera: pos=({:.2f}, {:.2f}, {:.2f}), fov={:.2f}", 
                                position.x, position.y, position.z, fov);
                        }
                    }
                }
                
                // Deserialize scene
                std::string sceneJsonStr = sceneJson.dump();
                if (gm::SceneSerializer::Deserialize(*scene, sceneJsonStr)) {
                    gm::core::Logger::Info("[DebugMenu] Scene loaded from: {}", filePath);
                    scene->Init();
                    if (m_callbacks.onSceneLoaded) {
                        m_callbacks.onSceneLoaded();
                    }
                    AddRecentFile(filePath);
                } else {
                    gm::core::Logger::Error("[DebugMenu] Failed to load scene from: {}", filePath);
                }
            }
        }
    } else {
        // File dialog was cancelled or failed - show fallback dialog on non-Windows
#ifndef _WIN32
        m_showLoadDialog = true;
        std::memset(m_filePathBuffer, 0, sizeof(m_filePathBuffer));
#endif
    }
}

void DebugMenu::RenderEditorWindow() {
    if (!ImGui::Begin("Scene Editor", &m_showInspector)) {
        ImGui::End();
        return;
    }

    // Split into two columns: Hierarchy on left, Inspector on right
    ImGui::BeginChild("Hierarchy", ImVec2(ImGui::GetContentRegionAvail().x * 0.3f, 0), false);
    RenderSceneHierarchy();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("Inspector", ImVec2(0, 0), false);
    RenderInspector();
    ImGui::EndChild();

    ImGui::End();
}

void DebugMenu::RenderSceneHierarchy() {
    auto scene = m_scene.lock();
    if (!scene) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No scene available");
        return;
    }

    ImGui::Text("Scene Hierarchy");
    ImGui::Separator();

    // Search filter
    static char searchFilter[256] = {0};
    ImGui::InputText("Search", searchFilter, sizeof(searchFilter));
    ImGui::Separator();

    // List all GameObjects
    auto allObjects = scene->GetAllGameObjects();
    std::string filterStr(searchFilter);
    std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), 
                   [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });

    int visibleCount = 0;
    for (const auto& gameObject : allObjects) {
        if (!gameObject || gameObject->IsDestroyed()) {
            continue;
        }

        std::string name = gameObject->GetName();
        if (name.empty()) {
            name = "Unnamed GameObject";
        }

        // Apply filter
        if (!filterStr.empty()) {
            std::string lowerName = name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                           [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
            if (lowerName.find(filterStr) == std::string::npos) {
                continue;
            }
        }

        visibleCount++;

        // Selection highlight
        auto selected = m_selectedGameObject.lock();
        bool isSelected = (selected && selected == gameObject);
        
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
        }

        // Display GameObject with active state indicator
        std::string displayName = name;
        if (!gameObject->IsActive()) {
            displayName += " [Inactive]";
        }

        if (ImGui::Selectable(displayName.c_str(), isSelected)) {
            m_selectedGameObject = gameObject;
        }

        if (isSelected) {
            ImGui::PopStyleColor();
        }
    }

    if (visibleCount == 0 && !filterStr.empty()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No GameObjects match filter");
    } else if (visibleCount == 0) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No GameObjects in scene");
    }
}

void DebugMenu::RenderInspector() {
    auto selected = m_selectedGameObject.lock();
    if (!selected) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No GameObject selected");
        ImGui::Text("Select a GameObject from the Scene Hierarchy");
        return;
    }

    // GameObject name
    char nameBuffer[256];
    const std::string& name = selected->GetName();
    std::size_t copyLen = std::min(name.length(), sizeof(nameBuffer) - 1);
    std::memcpy(nameBuffer, name.c_str(), copyLen);
    nameBuffer[copyLen] = '\0';
    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
        selected->SetName(nameBuffer);
    }

    // Active state
    bool isActive = selected->IsActive();
    if (ImGui::Checkbox("Active", &isActive)) {
        selected->SetActive(isActive);
        // Mark active lists as dirty when GameObject active state changes
        auto scene = m_scene.lock();
        if (scene) {
            scene->MarkActiveListsDirty();
        }
    }

    ImGui::Separator();

    // Transform Component
    auto transform = selected->GetTransform();
    if (transform) {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            glm::vec3 pos = transform->GetPosition();
            if (ImGui::DragFloat3("Position", glm::value_ptr(pos), 0.1f)) {
                transform->SetPosition(pos);
            }

            glm::vec3 rot = transform->GetRotation();
            if (ImGui::DragFloat3("Rotation", glm::value_ptr(rot), 1.0f)) {
                transform->SetRotation(rot);
            }

            glm::vec3 scale = transform->GetScale();
            if (ImGui::DragFloat3("Scale", glm::value_ptr(scale), 0.01f)) {
                transform->SetScale(scale);
            }
        }
    }

    // Other Components
    auto components = selected->GetComponents();
    for (const auto& component : components) {
        if (!component) {
            continue;
        }

        // Skip Transform as it's already shown
        if (component == transform) {
            continue;
        }

        std::string compName = component->GetName();
        if (compName.empty()) {
            compName = "Component";
        }

        if (ImGui::CollapsingHeader(compName.c_str())) {
            // StaticMeshComponent
            if (auto meshComp = std::dynamic_pointer_cast<gm::scene::StaticMeshComponent>(component)) {
                ImGui::Text("Mesh GUID: %s", meshComp->GetMeshGuid().empty() ? "None" : meshComp->GetMeshGuid().c_str());
                ImGui::Text("Shader GUID: %s", meshComp->GetShaderGuid().empty() ? "None" : meshComp->GetShaderGuid().c_str());
                ImGui::Text("Material GUID: %s", meshComp->GetMaterialGuid().empty() ? "None" : meshComp->GetMaterialGuid().c_str());
                ImGui::Text("Has Mesh: %s", meshComp->GetMesh() ? "Yes" : "No");
                ImGui::Text("Has Shader: %s", meshComp->GetShader() ? "Yes" : "No");
                ImGui::Text("Has Material: %s", meshComp->GetMaterial() ? "Yes" : "No");
            }
            // RigidBodyComponent
            else if (auto rigidBody = std::dynamic_pointer_cast<gm::physics::RigidBodyComponent>(component)) {
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
            }
            // LightComponent
            else if (auto light = std::dynamic_pointer_cast<gm::LightComponent>(component)) {
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
            }
            // EditableTerrainComponent
            else if (auto terrain = std::dynamic_pointer_cast<EditableTerrainComponent>(component)) {
                // Terrain Editor Controls
                bool editingEnabled = terrain->IsEditingEnabled();
                if (ImGui::Checkbox("Enable Editing", &editingEnabled)) {
                    terrain->SetEditingEnabled(editingEnabled);
                }
                ImGui::Separator();
                ImGui::TextUnformatted("Hold LMB to raise terrain.");
                ImGui::TextUnformatted("Hold RMB to lower terrain.");
                ImGui::Separator();
                
                // Brush settings
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
                
                // Height settings
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
                
                // Terrain size and resolution
                float terrainSize = terrain->GetTerrainSize();
                if (ImGui::SliderFloat("Terrain Size", &terrainSize, 5.0f, 100.0f, "%.1f m")) {
                    terrain->SetTerrainSize(terrainSize);
                }
                
                int resolution = terrain->GetResolution();
                if (ImGui::InputInt("Resolution", &resolution)) {
                    if (resolution != terrain->GetResolution() && resolution >= 2 && resolution <= 256) {
                        terrain->SetResolution(resolution);
                    }
                }
            }
            // Generic component info
            else {
                ImGui::Text("Component Type: %s", compName.c_str());
                ImGui::Text("Active: %s", component->IsActive() ? "Yes" : "No");
            }
        }
    }
}

void DebugMenu::RenderSceneInfo() {
    auto scene = m_scene.lock();
    if (!scene) {
        return;
    }

    if (!ImGui::Begin("Scene Info", &m_showSceneInfo)) {
        ImGui::End();
        return;
    }

    auto allObjects = scene->GetAllGameObjects();
    size_t activeCount = 0;
    size_t componentCount = 0;
    std::unordered_map<std::string, size_t> componentTypes;

    for (const auto& obj : allObjects) {
        if (!obj || obj->IsDestroyed()) {
            continue;
        }
        if (obj->IsActive()) {
            activeCount++;
        }
        auto components = obj->GetComponents();
        componentCount += components.size();
        for (const auto& comp : components) {
            if (comp) {
                std::string compName = comp->GetName();
                if (compName.empty()) {
                    compName = "Unknown";
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

void DebugMenu::AddRecentFile(const std::string& filePath) {
    if (filePath.empty()) {
        return;
    }

    // Remove if already exists
    m_recentFiles.erase(
        std::remove(m_recentFiles.begin(), m_recentFiles.end(), filePath),
        m_recentFiles.end()
    );

    // Add to front
    m_recentFiles.insert(m_recentFiles.begin(), filePath);

    // Limit to max
    if (m_recentFiles.size() > kMaxRecentFiles) {
        m_recentFiles.resize(kMaxRecentFiles);
    }

    // Save to disk
    SaveRecentFilesToDisk();
}

void DebugMenu::LoadRecentFile(const std::string& filePath) {
    if (!std::filesystem::exists(filePath)) {
        gm::core::Logger::Warning("[DebugMenu] Recent file does not exist: {}", filePath);
        // Remove from recent files
        m_recentFiles.erase(
            std::remove(m_recentFiles.begin(), m_recentFiles.end(), filePath),
            m_recentFiles.end()
        );
        SaveRecentFilesToDisk();
        return;
    }

    auto scene = m_scene.lock();
    if (!scene) {
        return;
    }

    // Load scene using the same logic as HandleLoad
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        gm::core::Logger::Error("[DebugMenu] Failed to open file: {}", filePath);
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string jsonStr = buffer.str();
    file.close();

    try {
        nlohmann::json sceneJson = nlohmann::json::parse(jsonStr);
        
        // Deserialize scene
        std::string sceneJsonStr = sceneJson.dump();
        if (gm::SceneSerializer::Deserialize(*scene, sceneJsonStr)) {
            gm::core::Logger::Info("[DebugMenu] Scene loaded from: {}", filePath);
            scene->Init();
            if (m_callbacks.onSceneLoaded) {
                m_callbacks.onSceneLoaded();
            }
            // Add to recent files (will move to top)
            AddRecentFile(filePath);
        } else {
            gm::core::Logger::Error("[DebugMenu] Failed to load scene from: {}", filePath);
        }
    } catch (const nlohmann::json::exception& e) {
        gm::core::Logger::Error("[DebugMenu] JSON parse error: {}", e.what());
    }
}

void DebugMenu::LoadRecentFilesFromDisk() {
    if (!std::filesystem::exists(m_recentFilesPath)) {
        return;
    }

    std::ifstream file(m_recentFilesPath);
    if (!file.is_open()) {
        return;
    }

    m_recentFiles.clear();
    std::string line;
    while (std::getline(file, line) && m_recentFiles.size() < kMaxRecentFiles) {
        if (!line.empty() && std::filesystem::exists(line)) {
            m_recentFiles.push_back(line);
        }
    }
    file.close();
}

void DebugMenu::SaveRecentFilesToDisk() {
    // Ensure directory exists
    std::filesystem::path path(m_recentFilesPath);
    std::filesystem::path dir = path.parent_path();
    if (!dir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec) {
            gm::core::Logger::Error("[DebugMenu] Failed to create directory for recent files: {}", 
                        dir.string());
            return;
        }
    }

    std::ofstream file(m_recentFilesPath);
    if (!file.is_open()) {
        gm::core::Logger::Error("[DebugMenu] Failed to save recent files to: {}", 
                    m_recentFilesPath);
        return;
    }

    for (const auto& filePath : m_recentFiles) {
        file << filePath << "\n";
    }
    file.close();
}

