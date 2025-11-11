#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <windows.h>
#include <GLFW/glfw3native.h>
#endif

#include "Game.hpp"
#include "GameSceneHelpers.hpp"
#include "SceneSerializerExtensions.hpp"
#include "gm/physics/PhysicsWorld.hpp"
#include "EditableTerrainComponent.hpp"

#include <cstdio>
#include <filesystem>
#include <optional>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "gm/rendering/Camera.hpp"
#include "gm/scene/Transform.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/scene/SceneManager.hpp"
#include "gm/rendering/Material.hpp"
#include "gm/core/Input.hpp"
#include "gm/core/InputBindings.hpp"
#include "gm/core/input/InputManager.hpp"
#include "gm/core/Logger.hpp"
#include "gm/save/SaveManager.hpp"
#include "gm/save/SaveSnapshotHelpers.hpp"
#include "gm/utils/ImGuiManager.hpp"
#include "gm/utils/HotReloader.hpp"
#include "gm/tooling/Overlay.hpp"
#include "gm/gameplay/FlyCameraController.hpp"
#ifdef _DEBUG
#include "DebugMenu.hpp"
#endif
#include "EditableTerrainComponent.hpp"
#include <imgui.h>


Game::Game(const gm::utils::AppConfig& config)
    : m_config(config),
      m_assetsDir(config.paths.assets) {}

Game::~Game() = default;

bool Game::Init(GLFWwindow* window) {
    m_window = window;
    if (!m_window) {
        gm::core::Logger::Error("[Game] Invalid window handle");
        return false;
    }

    auto& physics = gm::physics::PhysicsWorld::Instance();
    if (!physics.IsInitialized()) {
        physics.Init();
    }

    const std::string assetsPath = m_assetsDir.string();
    if (!m_resources.Load(assetsPath)) {
        gm::core::Logger::Error("[Game] Failed to load resources from %s", assetsPath.c_str());
        return false;
    }

    gm::SceneSerializerExtensions::RegisterSerializers();

    m_camera = std::make_unique<gm::Camera>();

    // Set up input action bindings
    auto& inputManager = gm::core::InputManager::Instance();
    gm::core::InputBindings::SetupDefaultBindings(inputManager);

    SetupScene();
    ApplyResourcesToScene();

    m_gameplay = std::make_unique<gm::gameplay::FlyCameraController>(*m_camera, m_window);
    m_gameplay->SetScene(m_gameScene);

    m_saveManager = std::make_unique<gm::save::SaveManager>(m_config.paths.saves);

    m_imgui = std::make_unique<gm::utils::ImGuiManager>();
    if (m_imgui && !m_imgui->Init(m_window)) {
        gm::core::Logger::Warning("[Game] Failed to initialize ImGui; tooling overlay disabled");
        m_imgui.reset();
    }

    m_tooling = std::make_unique<gm::tooling::Overlay>();
    if (m_tooling) {
        gm::tooling::Overlay::Callbacks callbacks{
            [this]() { PerformQuickSave(); },
            [this]() { PerformQuickLoad(); },
            [this]() { ForceResourceReload(); }
        };
        m_tooling->SetCallbacks(std::move(callbacks));
        m_tooling->SetSaveManager(m_saveManager.get());
        m_tooling->SetHotReloader(&m_hotReloader);
        m_tooling->SetCamera(m_camera.get());
        m_tooling->SetScene(m_gameScene);
        m_tooling->SetPhysicsWorld(&gm::physics::PhysicsWorld::Instance());
        m_tooling->SetWorldInfoProvider([this]() -> std::optional<gm::tooling::Overlay::WorldInfo> {
            if (!m_gameplay || !m_camera) return std::nullopt;
            gm::tooling::Overlay::WorldInfo info;
            info.sceneName = m_gameplay->GetActiveSceneName();
            info.worldTimeSeconds = m_gameplay->GetWorldTimeSeconds();
            info.cameraPosition = m_camera->Position();
            info.cameraDirection = m_camera->Front();
            return info;
        });
        m_tooling->AddNotification("Tooling overlay ready");
    }

#ifdef _DEBUG
    m_debugMenu = std::make_unique<DebugMenu>();
    if (m_debugMenu) {
        DebugMenu::Callbacks callbacks{
            [this]() { PerformQuickSave(); },
            [this]() { PerformQuickLoad(); },
            [this]() { ForceResourceReload(); },
            [this]() {
                gm::core::Logger::Info("[Game] onSceneLoaded callback called");
                
                // Log all GameObjects in scene before applying resources
                if (m_gameScene) {
                    auto allObjects = m_gameScene->GetAllGameObjects();
                    gm::core::Logger::Info("[Game] Scene has %zu GameObjects after load", allObjects.size());
                    for (const auto& obj : allObjects) {
                        if (obj) {
                            gm::core::Logger::Info("[Game]   - GameObject: '%s' (active=%s)", 
                                obj->GetName().c_str(), obj->IsActive() ? "yes" : "no");
                            auto components = obj->GetComponents();
                            gm::core::Logger::Info("[Game]     Components: %zu", components.size());
                            for (const auto& comp : components) {
                                if (comp) {
                                    gm::core::Logger::Info("[Game]       - %s (active=%s)", 
                                        comp->GetName().c_str(), comp->IsActive() ? "yes" : "no");
                                }
                            }
                        }
                    }
                }
                
                ApplyResourcesToScene();
                // Reconnect terrain component after scene load
                if (m_debugMenu && m_gameScene) {
                    auto terrainObject = m_gameScene->FindGameObjectByName("Terrain");
                    if (terrainObject) {
                        if (auto terrain = terrainObject->GetComponent<EditableTerrainComponent>()) {
                            m_debugMenu->SetTerrainComponent(terrain.get());
                            // Ensure terrain editor window can be opened
                            // Don't force it open, but make sure it's connected
                        }
                    }
                }
            },
            // Camera getters
            [this]() -> glm::vec3 {
                return m_camera ? m_camera->Position() : glm::vec3(0.0f);
            },
            [this]() -> glm::vec3 {
                return m_camera ? m_camera->Front() : glm::vec3(0.0f, 0.0f, -1.0f);
            },
            [this]() -> float {
                return m_gameplay ? m_gameplay->GetFovDegrees() : 60.0f;
            },
            // Camera setter
            [this](const glm::vec3& position, const glm::vec3& forward, float fov) {
                if (m_camera) {
                    m_camera->SetPosition(position);
                    m_camera->SetForward(forward);
                }
                if (m_gameplay && fov > 0.0f) {
                    m_gameplay->SetFovDegrees(fov);
                }
            }
        };
        m_debugMenu->SetCallbacks(std::move(callbacks));
        m_debugMenu->SetSaveManager(m_saveManager.get());
        m_debugMenu->SetScene(m_gameScene);
        
        // Set window handle for file dialogs
#ifdef _WIN32
        if (m_window) {
            HWND hwnd = glfwGetWin32Window(m_window);
            m_debugMenu->SetWindowHandle(hwnd);
        }
#endif

        // Find terrain component and connect it to debug menu
        if (m_gameScene) {
            auto terrainObject = m_gameScene->FindGameObjectByName("Terrain");
            if (terrainObject) {
                if (auto terrain = terrainObject->GetComponent<EditableTerrainComponent>()) {
                    m_debugMenu->SetTerrainComponent(terrain.get());
                }
            }
        }
    }
#endif

    SetupResourceHotReload();
    return true;
}

void Game::SetupScene() {
    auto& sceneManager = gm::SceneManager::Instance();

    m_gameScene = sceneManager.LoadScene("GameScene");
    if (!m_gameScene) {
        std::printf("[Game] Error: Failed to create game scene\n");
        return;
    }

    m_gameScene->SetParallelGameObjectUpdates(true);
    std::printf("[Game] Game scene initialized successfully\n");

    // Populate initial scene if empty
    if (m_gameScene->GetAllGameObjects().empty()) {
        auto fovProvider = [this]() -> float {
            return m_gameplay ? m_gameplay->GetFovDegrees() : 60.0f;
        };
        gotmilked::PopulateInitialScene(*m_gameScene, *m_camera, m_resources, m_window, fovProvider);
    }
    ApplyResourcesToScene();

    if (m_gameplay) {
        m_gameplay->SetScene(m_gameScene);
    }
}

void Game::Update(float dt) {
    if (!m_window) return;

    auto& physics = gm::physics::PhysicsWorld::Instance();
    if (physics.IsInitialized()) {
        physics.Step(dt);
    }

    auto& input = gm::core::Input::Instance();

    if (input.IsActionJustPressed("Exit")) {
        glfwSetWindowShouldClose(m_window, 1);
    }

    if (input.IsActionJustPressed("QuickSave")) {
        PerformQuickSave();
    }

    if (input.IsActionJustPressed("QuickLoad")) {
        PerformQuickLoad();
    }

    if (input.IsActionJustPressed("ToggleOverlay")) {
#ifdef _DEBUG
        if (m_imgui && m_imgui->IsInitialized()) {
            m_debugMenuVisible = !m_debugMenuVisible;
        } else {
            gm::core::Logger::Warning("[Game] ImGui not initialized; debug menu not available");
        }
#endif
    }

    if (m_gameplay) {
        m_gameplay->SetWindow(m_window);
        m_gameplay->SetScene(m_gameScene);
        
        // Only suppress input if ImGui wants to capture it (typing in fields, etc.)
        bool imguiWantsInput = false;
        if (m_imgui && m_imgui->IsInitialized()) {
            ImGuiIO& io = ImGui::GetIO();
            imguiWantsInput = io.WantCaptureKeyboard || io.WantCaptureMouse;
        }
        
        m_gameplay->SetInputSuppressed(imguiWantsInput || m_overlayVisible);
        m_gameplay->Update(dt);
    }

    m_hotReloader.Update(dt);
}


void Game::Render() {
    if (!m_window) return;
    if (!m_resources.shader) {
        std::printf("[Game] Warning: Cannot render - shader not loaded\n");
        return;
    }

    int fbw, fbh;
    glfwGetFramebufferSize(m_window, &fbw, &fbh);
    glViewport(0, 0, fbw, fbh);
    glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = (float)fbw / (float)fbh;
    float fov = m_gameplay ? m_gameplay->GetFovDegrees() : 60.0f;
    glm::mat4 proj = glm::perspective(glm::radians(fov), aspect, 0.1f, 200.0f);
    glm::mat4 view = m_camera->View();

    if (m_imgui && m_imgui->IsInitialized()) {
        m_imgui->NewFrame();
    }

    // Render the game scene with all its GameObjects
    if (m_gameScene && m_camera) {
        m_gameScene->Draw(*m_resources.shader, *m_camera, fbw, fbh, fov);
    }
    
    if (m_imgui && m_imgui->IsInitialized()) {
#ifdef _DEBUG
        if (m_debugMenu) {
            m_debugMenu->Render(m_debugMenuVisible);
        }
#endif
        if (m_tooling) {
            bool open = m_overlayVisible;
            m_tooling->Render(open);
            m_overlayVisible = open;
        }
        m_imgui->Render();
    }
}

void Game::Shutdown() {
    // Clean up scene and scene manager
    m_gameScene.reset();

    gm::SceneSerializerExtensions::UnregisterSerializers();
    m_resources.Release();
    
    m_gameplay.reset();
    m_saveManager.reset();
    if (m_imgui) {
        m_imgui->Shutdown();
        m_imgui.reset();
    }
    m_tooling.reset();
#ifdef _DEBUG
    m_debugMenu.reset();
#endif
    m_camera.reset();
    
    // InputManager is a singleton, no need to manually reset
    printf("[Game] Shutdown complete\n");

    gm::physics::PhysicsWorld::Instance().Shutdown();
}

void Game::SetupResourceHotReload() {
    m_hotReloader.SetEnabled(m_config.hotReload.enable);
    m_hotReloader.SetPollInterval(m_config.hotReload.pollIntervalSeconds);

    if (!m_config.hotReload.enable) {
        return;
    }

    if (!m_resources.shaderVertPath.empty() && !m_resources.shaderFragPath.empty()) {
        m_hotReloader.AddWatch(
            "game_shader",
            {std::filesystem::path(m_resources.shaderVertPath), std::filesystem::path(m_resources.shaderFragPath)},
            [this]() {
                bool ok = m_resources.ReloadShader();
                if (ok) {
                    ApplyResourcesToScene();
                }
                return ok;
            });
    }

    if (!m_resources.texturePath.empty()) {
        m_hotReloader.AddWatch(
            "game_texture",
            {std::filesystem::path(m_resources.texturePath)},
            [this]() {
                bool ok = m_resources.ReloadTexture();
                if (ok) {
                    ApplyResourcesToScene();
                }
                return ok;
            });
    }

    if (!m_resources.meshPath.empty()) {
        m_hotReloader.AddWatch(
            "game_mesh",
            {std::filesystem::path(m_resources.meshPath)},
            [this]() {
                bool ok = m_resources.ReloadMesh();
                if (ok) {
                    ApplyResourcesToScene();
                }
                return ok;
            });
    }

    m_hotReloader.ForcePoll();
}

void Game::ApplyResourcesToScene() {
    if (!m_gameScene) {
        gm::core::Logger::Warning("[Game] ApplyResourcesToScene: No scene available");
        return;
    }

    auto terrainObject = m_gameScene->FindGameObjectByName("Terrain");
    if (!terrainObject) {
        gm::core::Logger::Warning("[Game] ApplyResourcesToScene: Terrain object not found");
        return;
    }

    if (auto terrain = terrainObject->GetComponent<EditableTerrainComponent>()) {
        gm::core::Logger::Info("[Game] ApplyResourcesToScene: Found terrain component");
        
        terrain->SetShader(m_resources.shader.get());
        terrain->SetMaterial(m_resources.planeMaterial);
        terrain->SetWindow(m_window);
        terrain->SetCamera(m_camera.get());
        
        // Set FOV provider for terrain ray picking
        if (m_gameplay) {
            auto fovProvider = [this]() -> float {
                return m_gameplay->GetFovDegrees();
            };
            terrain->SetFovProvider(std::move(fovProvider));
        }
        
        // Ensure terrain GameObject is initialized
        terrainObject->Init();
        
        // Reinitialize terrain component to rebuild mesh if needed
        // This will mark mesh as dirty if heights are already loaded
        terrain->Init();
        
        // After loading scenes, explicitly mark mesh as dirty to ensure it rebuilds
        // This is critical because the terrain component might have been deserialized
        // with height data, but the mesh hasn't been created yet
        terrain->MarkMeshDirty();
        
        // Log terrain state for debugging
        gm::core::Logger::Info("[Game] Terrain: resolution=%d, size=%.2f, heights=%zu, shader=%s, material=%s",
            terrain->GetResolution(),
            terrain->GetTerrainSize(),
            terrain->GetHeights().size(),
            m_resources.shader ? "set" : "null",
            m_resources.planeMaterial ? "set" : "null");
    } else {
        gm::core::Logger::Warning("[Game] ApplyResourcesToScene: Terrain component not found on Terrain object");
    }

    if (m_tooling) {
        m_tooling->SetScene(m_gameScene);
    }
}

void Game::PerformQuickSave() {
    if (!m_saveManager || !m_gameScene || !m_camera || !m_gameplay) {
        gm::core::Logger::Warning("[Game] QuickSave unavailable (missing dependencies)");
        if (m_tooling) m_tooling->AddNotification("QuickSave unavailable");
        return;
    }

    auto data = gm::save::SaveSnapshotHelpers::CaptureSnapshot(
        m_camera.get(),
        m_gameScene,
        [this]() { return m_gameplay->GetWorldTimeSeconds(); });
    
    // Add FOV to save data
    data.cameraFov = m_gameplay->GetFovDegrees();

    if (m_gameScene) {
        auto terrainObject = m_gameScene->FindGameObjectByName("Terrain");
        if (terrainObject) {
            if (auto terrain = terrainObject->GetComponent<EditableTerrainComponent>()) {
                data.terrainResolution = terrain->GetResolution();
                data.terrainSize = terrain->GetTerrainSize();
                data.terrainMinHeight = terrain->GetMinHeight();
                data.terrainMaxHeight = terrain->GetMaxHeight();
                data.terrainHeights = terrain->GetHeights();
            }
        }
    }

    auto result = m_saveManager->QuickSave(data);
    if (!result.success) {
        gm::core::Logger::Warning("[Game] QuickSave failed: %s", result.message.c_str());
        if (m_tooling) m_tooling->AddNotification("QuickSave failed");
    } else {
        gm::core::Logger::Info("[Game] QuickSave completed");
        if (m_tooling) m_tooling->AddNotification("QuickSave completed");
    }
}

void Game::PerformQuickLoad() {
    if (!m_saveManager || !m_gameScene || !m_camera || !m_gameplay) {
        gm::core::Logger::Warning("[Game] QuickLoad unavailable (missing dependencies)");
        if (m_tooling) m_tooling->AddNotification("QuickLoad unavailable");
        return;
    }

    gm::save::SaveGameData data;
    auto result = m_saveManager->QuickLoad(data);
    if (!result.success) {
        gm::core::Logger::Warning("[Game] QuickLoad failed: %s", result.message.c_str());
        if (m_tooling) m_tooling->AddNotification("QuickLoad failed");
        return;
    }

    bool applied = gm::save::SaveSnapshotHelpers::ApplySnapshot(
        data,
        m_camera.get(),
        m_gameScene,
        [this](double worldTime) {
            if (m_gameplay) {
                m_gameplay->SetWorldTimeSeconds(worldTime);
            }
        });
    
    if (applied && m_gameScene) {
        if (data.terrainResolution > 0 && !data.terrainHeights.empty()) {
            auto terrainObject = m_gameScene->FindGameObjectByName("Terrain");
            if (terrainObject) {
                if (auto terrain = terrainObject->GetComponent<EditableTerrainComponent>()) {
                    bool ok = terrain->SetHeightData(
                        data.terrainResolution,
                        data.terrainSize,
                        data.terrainMinHeight,
                        data.terrainMaxHeight,
                        data.terrainHeights);
                    if (!ok) {
                        gm::core::Logger::Warning("[Game] Failed to apply terrain data from save");
                    } else {
                        // SetHeightData already marks mesh as dirty, but ensure it's marked
                        terrain->MarkMeshDirty();
                    }
                }
            }
        }
    }
    
    // Apply FOV if present
    if (data.cameraFov > 0.0f && m_gameplay) {
        m_gameplay->SetFovDegrees(data.cameraFov);
    }
    
    ApplyResourcesToScene();
    if (m_tooling) {
        m_tooling->AddNotification(applied ? "QuickLoad applied" : "QuickLoad partially applied");
    }
}

void Game::ForceResourceReload() {
    bool ok = m_resources.ReloadAll();
    ApplyResourcesToScene();
    m_hotReloader.ForcePoll();
    if (ok) {
        gm::core::Logger::Info("[Game] Resources reloaded");
        if (m_tooling) m_tooling->AddNotification("Resources reloaded");
    } else {
        gm::core::Logger::Warning("[Game] Resource reload encountered errors");
        if (m_tooling) m_tooling->AddNotification("Resource reload failed");
    }
}

