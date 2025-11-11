#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <windows.h>
#include <GLFW/glfw3native.h>
#endif

#include "Game.hpp"
#include "GameSceneHelpers.hpp"
#include "GameConstants.hpp"
#include "GameEvents.hpp"
#include "SceneSerializerExtensions.hpp"
#include "gm/scene/SceneSerializer.hpp"
#include "EditableTerrainComponent.hpp"

#include <filesystem>
#include <optional>
#include <glm/gtc/matrix_transform.hpp>
#include <nlohmann/json.hpp>

#include "gm/rendering/Camera.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/scene/SceneManager.hpp"
#include "gm/scene/StaticMeshComponent.hpp"
#include "gm/physics/PhysicsWorld.hpp"
#include "gm/rendering/Material.hpp"
#include "gm/core/Input.hpp"
#include "gm/core/InputBindings.hpp"
#include "gm/core/input/InputManager.hpp"
#include "gm/core/Logger.hpp"
#include "gm/core/Event.hpp"
#include "gm/save/SaveManager.hpp"
#include "gm/save/SaveSnapshotHelpers.hpp"
#include "gm/utils/ImGuiManager.hpp"
#include "gm/utils/HotReloader.hpp"
#include "gm/tooling/Overlay.hpp"
#include "gm/gameplay/FlyCameraController.hpp"
#ifdef _DEBUG
#include "DebugMenu.hpp"
#endif
#include <imgui.h>


Game::Game(const gm::utils::AppConfig& config)
    : m_config(config),
      m_assetsDir(config.paths.assets) {}

Game::~Game() = default;

bool Game::Init(GLFWwindow* window) {
    m_window = window;
    m_vsyncEnabled = m_config.window.vsync;  // Initialize VSync state from config
    if (!m_window) {
        gm::core::Logger::Error("[Game] Invalid window handle");
        return false;
    }

    if (!SetupPhysics()) {
        return false;
    }

    if (!SetupRendering()) {
        return false;
    }

    SetupInput();
    SetupScene();
    ApplyResourcesToScene();
    SetupGameplay();
    SetupSaveSystem();

    if (!SetupDebugTools()) {
        gm::core::Logger::Warning("[Game] Some debug tools failed to initialize, continuing anyway");
    }

    SetupResourceHotReload();
    SetupEventSubscriptions();
    
    // Trigger initialization event
    gm::core::Event::Trigger(gotmilked::GameEvents::GameInitialized);
    return true;
}

bool Game::SetupPhysics() {
    auto& physics = gm::physics::PhysicsWorld::Instance();
    if (!physics.IsInitialized()) {
        try {
            physics.Init();
        } catch (const std::exception& ex) {
            gm::core::Logger::Error("[Game] Failed to initialize physics: %s", ex.what());
            return false;
        } catch (...) {
            gm::core::Logger::Error("[Game] Failed to initialize physics: unknown error");
            return false;
        }
    }
    
    if (!physics.IsInitialized()) {
        gm::core::Logger::Error("[Game] Physics initialization completed but IsInitialized() returned false");
        return false;
    }
    
    return true;
}

bool Game::SetupRendering() {
    if (!m_resources.Load(m_assetsDir, m_config.resources)) {
        gm::core::Logger::Error("[Game] Failed to load resources from %s", m_assetsDir.string().c_str());
        return false;
    }

    gm::SceneSerializerExtensions::RegisterSerializers();
    m_camera = std::make_unique<gm::Camera>();
    return true;
}

void Game::SetupInput() {
    auto& inputManager = gm::core::InputManager::Instance();
    gm::core::InputBindings::SetupDefaultBindings(inputManager);
}

void Game::SetupGameplay() {
    m_gameplay = std::make_unique<gm::gameplay::FlyCameraController>(*m_camera, m_window);
    m_gameplay->SetScene(m_gameScene);
}

void Game::SetupSaveSystem() {
    m_saveManager = std::make_unique<gm::save::SaveManager>(m_config.paths.saves);
}

bool Game::SetupDebugTools() {
    // Setup ImGui
    m_imgui = std::make_unique<gm::utils::ImGuiManager>();
    if (m_imgui && !m_imgui->Init(m_window)) {
        gm::core::Logger::Warning("[Game] Failed to initialize ImGui; tooling overlay disabled");
        m_imgui.reset();
        return false;
    }

    // Setup tooling overlay
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
    // Setup debug menu
    m_debugMenu = std::make_unique<DebugMenu>();
    if (m_debugMenu) {
        SetupDebugMenu();
    }
#endif

    return true;
}

#ifdef _DEBUG
void Game::SetupDebugMenu() {
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
            return m_gameplay ? m_gameplay->GetFovDegrees() : gotmilked::GameConstants::Camera::DefaultFovDegrees;
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
        },
        // World time getter
        [this]() -> double {
            return m_gameplay ? m_gameplay->GetWorldTimeSeconds() : 0.0;
        },
        // Rendering callbacks for GameObject labels
        [this]() -> glm::mat4 {
            return m_camera ? m_camera->View() : glm::mat4(1.0f);
        },
        [this]() -> glm::mat4 {
            if (!m_window || !m_gameplay) {
                return glm::mat4(1.0f);
            }
            int fbw, fbh;
            glfwGetFramebufferSize(m_window, &fbw, &fbh);
            if (fbw <= 0 || fbh <= 0) {
                return glm::mat4(1.0f);
            }
            float aspect = static_cast<float>(fbw) / static_cast<float>(fbh);
            float fov = m_gameplay->GetFovDegrees();
            return glm::perspective(
                glm::radians(fov), 
                aspect, 
                gotmilked::GameConstants::Camera::NearPlane, 
                gotmilked::GameConstants::Camera::FarPlane);
        },
        [this](int& width, int& height) {
            if (m_window) {
                glfwGetFramebufferSize(m_window, &width, &height);
            } else {
                width = 0;
                height = 0;
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
    
    // Load recent files from disk
    m_debugMenu->LoadRecentFilesFromDisk();
}
#endif

void Game::SetupScene() {
    auto& sceneManager = gm::SceneManager::Instance();

    m_gameScene = sceneManager.LoadScene("GameScene");
    if (!m_gameScene) {
        gm::core::Logger::Error("[Game] Failed to create game scene");
        return;
    }

    m_gameScene->SetParallelGameObjectUpdates(true);
    gm::core::Logger::Info("[Game] Game scene initialized successfully");

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
        // Flush any pending body operations before stepping
        physics.FlushPendingOperations();
        physics.Step(dt);
    }

    auto& input = gm::core::Input::Instance();
    
    // Handle V key to toggle VSync
    auto* inputSys = input.GetInputSystem();
    if (inputSys && inputSys->IsKeyJustPressed(GLFW_KEY_V)) {
        // Check if ImGui wants keyboard input (don't trigger if typing in a field)
        bool imguiWantsInput = false;
        if (m_imgui && m_imgui->IsInitialized()) {
            ImGuiIO& io = ImGui::GetIO();
            imguiWantsInput = io.WantCaptureKeyboard;
        }
        
        if (!imguiWantsInput) {
            m_vsyncEnabled = !m_vsyncEnabled;
            glfwSwapInterval(m_vsyncEnabled ? 1 : 0);
            gm::core::Logger::Info("[Game] VSync %s", m_vsyncEnabled ? "enabled" : "disabled");
        }
    }

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

#ifdef _DEBUG
    // Handle Ctrl+S (Save Scene As) and Ctrl+O (Load Scene From) shortcuts
    if (m_debugMenu && m_imgui && m_imgui->IsInitialized()) {
        auto* inputSys = input.GetInputSystem();
        if (inputSys) {
            // Check for Ctrl+S (Save Scene As)
            bool ctrlPressed = inputSys->IsKeyPressed(GLFW_KEY_LEFT_CONTROL) || 
                              inputSys->IsKeyPressed(GLFW_KEY_RIGHT_CONTROL);
            if (ctrlPressed && inputSys->IsKeyJustPressed(GLFW_KEY_S)) {
                // Check if ImGui wants keyboard input (don't trigger if typing in a field)
                ImGuiIO& io = ImGui::GetIO();
                if (!io.WantCaptureKeyboard) {
                    m_debugMenu->TriggerSaveAs();
                }
            }
            
            // Check for Ctrl+O (Load Scene From)
            if (ctrlPressed && inputSys->IsKeyJustPressed(GLFW_KEY_O)) {
                ImGuiIO& io = ImGui::GetIO();
                if (!io.WantCaptureKeyboard) {
                    m_debugMenu->TriggerLoad();
                }
            }
        }
    }
#endif

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
    if (!m_resources.GetShader()) {
        gm::core::Logger::Warning("[Game] Cannot render - shader not loaded");
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
        m_gameScene->Draw(*m_resources.GetShader(), *m_camera, fbw, fbh, fov);
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
    gm::core::Logger::Info("[Game] Shutdown complete");

    gm::physics::PhysicsWorld::Instance().Shutdown();
    
    // Trigger shutdown event
    gm::core::Event::Trigger(gotmilked::GameEvents::GameShutdown);
}

void Game::SetupEventSubscriptions() {
    // Subscribe to resource events for logging and notifications
        gm::core::Event::Subscribe(gotmilked::GameEvents::ResourceShaderLoaded, [this]() {
        gm::core::Logger::Debug("[Game] Event: Shader loaded");
        if (m_tooling) {
            m_tooling->AddNotification("Shader loaded");
        }
    });

        gm::core::Event::Subscribe(gotmilked::GameEvents::ResourceShaderReloaded, [this]() {
        gm::core::Logger::Debug("[Game] Event: Shader reloaded");
        if (m_tooling) {
            m_tooling->AddNotification("Shader reloaded");
        }
    });

        gm::core::Event::Subscribe(gotmilked::GameEvents::ResourceTextureLoaded, [this]() {
        gm::core::Logger::Debug("[Game] Event: Texture loaded");
    });

        gm::core::Event::Subscribe(gotmilked::GameEvents::ResourceTextureReloaded, [this]() {
        gm::core::Logger::Debug("[Game] Event: Texture reloaded");
        if (m_tooling) {
            m_tooling->AddNotification("Texture reloaded");
        }
    });

        gm::core::Event::Subscribe(gotmilked::GameEvents::ResourceMeshLoaded, [this]() {
        gm::core::Logger::Debug("[Game] Event: Mesh loaded");
    });

        gm::core::Event::Subscribe(gotmilked::GameEvents::ResourceMeshReloaded, [this]() {
        gm::core::Logger::Debug("[Game] Event: Mesh reloaded");
        if (m_tooling) {
            m_tooling->AddNotification("Mesh reloaded");
        }
    });

        gm::core::Event::Subscribe(gotmilked::GameEvents::ResourceAllReloaded, [this]() {
        gm::core::Logger::Info("[Game] Event: All resources reloaded");
        if (m_tooling) {
            m_tooling->AddNotification("All resources reloaded");
        }
    });

        gm::core::Event::Subscribe(gotmilked::GameEvents::ResourceLoadFailed, [this]() {
        gm::core::Logger::Warning("[Game] Event: Resource load failed");
        if (m_tooling) {
            m_tooling->AddNotification("Resource load failed");
        }
    });

        gm::core::Event::Subscribe(gotmilked::GameEvents::SceneQuickSaved, [this]() {
        gm::core::Logger::Debug("[Game] Event: Scene quick saved");
    });

        gm::core::Event::Subscribe(gotmilked::GameEvents::SceneQuickLoaded, [this]() {
        gm::core::Logger::Debug("[Game] Event: Scene quick loaded");
    });

        gm::core::Event::Subscribe(gotmilked::GameEvents::GameInitialized, [this]() {
        gm::core::Logger::Info("[Game] Event: Game initialized");
    });

        gm::core::Event::Subscribe(gotmilked::GameEvents::GameShutdown, [this]() {
        gm::core::Logger::Info("[Game] Event: Game shutdown");
    });
}

void Game::SetupResourceHotReload() {
    m_hotReloader.SetEnabled(m_config.hotReload.enable);
    m_hotReloader.SetPollInterval(m_config.hotReload.pollIntervalSeconds);

    if (!m_config.hotReload.enable) {
        return;
    }

    if (!m_resources.GetShaderVertPath().empty() && !m_resources.GetShaderFragPath().empty()) {
        m_hotReloader.AddWatch(
            "game_shader",
            {std::filesystem::path(m_resources.GetShaderVertPath()), std::filesystem::path(m_resources.GetShaderFragPath())},
            [this]() {
                gm::core::Event::Trigger(gotmilked::GameEvents::HotReloadShaderDetected);
                bool ok = m_resources.ReloadShader();
                if (ok) {
                    ApplyResourcesToScene();
                    gm::core::Event::Trigger(gotmilked::GameEvents::HotReloadShaderReloaded);
                }
                return ok;
            });
    }

    if (!m_resources.GetTexturePath().empty()) {
        m_hotReloader.AddWatch(
            "game_texture",
            {std::filesystem::path(m_resources.GetTexturePath())},
            [this]() {
                gm::core::Event::Trigger(gotmilked::GameEvents::HotReloadTextureDetected);
                bool ok = m_resources.ReloadTexture();
                if (ok) {
                    ApplyResourcesToScene();
                    gm::core::Event::Trigger(gotmilked::GameEvents::HotReloadTextureReloaded);
                }
                return ok;
            });
    }

    if (!m_resources.GetMeshPath().empty()) {
        m_hotReloader.AddWatch(
            "game_mesh",
            {std::filesystem::path(m_resources.GetMeshPath())},
            [this]() {
                gm::core::Event::Trigger(gotmilked::GameEvents::HotReloadMeshDetected);
                bool ok = m_resources.ReloadMesh();
                if (ok) {
                    ApplyResourcesToScene();
                    gm::core::Event::Trigger(gotmilked::GameEvents::HotReloadMeshReloaded);
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

    ApplyResourcesToTerrain();
    ApplyResourcesToStaticMeshComponents();

    if (m_tooling) {
        m_tooling->SetScene(m_gameScene);
    }
}

void Game::ApplyResourcesToStaticMeshComponents() {
    if (!m_gameScene) {
        return;
    }

    // Create resolver functions that map GUIDs to resource objects
    auto meshResolver = [this](const std::string& guid) -> gm::Mesh* {
        if (guid == m_resources.GetMeshGuid()) {
            return m_resources.GetMesh();
        }
        // Could add more mesh GUID mappings here
        return nullptr;
    };

    auto shaderResolver = [this](const std::string& guid) -> gm::Shader* {
        if (guid == m_resources.GetShaderGuid()) {
            return m_resources.GetShader();
        }
        // Could add more shader GUID mappings here
        return nullptr;
    };

    auto materialResolver = [this](const std::string& /*guid*/) -> std::shared_ptr<gm::Material> {
        // For now, materials don't have GUIDs in GameResources
        // This could be extended later to support material GUIDs
        // For now, return nullptr and let components use default materials
        return nullptr;
    };

    // Find all StaticMeshComponents and restore their resources
    for (const auto& gameObject : m_gameScene->GetAllGameObjects()) {
        if (!gameObject || !gameObject->IsActive()) {
            continue;
        }

        auto meshComp = gameObject->GetComponent<gm::scene::StaticMeshComponent>();
        if (meshComp) {
            // Only restore if component has GUIDs but no resources
            if ((!meshComp->GetMeshGuid().empty() && !meshComp->GetMesh()) ||
                (!meshComp->GetShaderGuid().empty() && !meshComp->GetShader())) {
                meshComp->RestoreResources(meshResolver, shaderResolver, materialResolver);
                gm::core::Logger::Info("[Game] Restored resources for StaticMeshComponent on GameObject '%s'",
                    gameObject->GetName().c_str());
            }
        }
    }
}

void Game::ApplyResourcesToTerrain() {
    if (!m_gameScene) {
        return;
    }

    auto terrainObject = m_gameScene->FindGameObjectByName("Terrain");
    if (!terrainObject) {
        return;
    }

    auto terrain = terrainObject->GetComponent<EditableTerrainComponent>();
    if (!terrain) {
        return;
    }

    // Apply resources to terrain component
    terrain->SetShader(m_resources.GetShader());
    terrain->SetMaterial(m_resources.GetTerrainMaterial());
    terrain->SetWindow(m_window);
    terrain->SetCamera(m_camera.get());

    // Set FOV provider for terrain ray picking
    if (m_gameplay) {
        terrain->SetFovProvider([this]() -> float {
            return m_gameplay->GetFovDegrees();
        });
    }

    // Initialize terrain and mark mesh dirty to ensure it rebuilds
    terrainObject->Init();
    terrain->Init();
    terrain->MarkMeshDirty();
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

    // Serialize the scene to include all GameObjects and their properties
    std::string sceneJsonString = gm::SceneSerializer::Serialize(*m_gameScene);
    nlohmann::json sceneJson = nlohmann::json::parse(sceneJsonString);
    
    // Merge SaveGameData into the scene JSON
    nlohmann::json saveJson = {
        {"version", data.version},
        {"sceneName", data.sceneName},
        {"camera", {
            {"position", {data.cameraPosition.x, data.cameraPosition.y, data.cameraPosition.z}},
            {"forward", {data.cameraForward.x, data.cameraForward.y, data.cameraForward.z}},
            {"fov", data.cameraFov}
        }},
        {"worldTime", data.worldTime}
    };
    
    if (data.terrainResolution > 0 && !data.terrainHeights.empty()) {
        saveJson["terrain"] = {
            {"resolution", data.terrainResolution},
            {"size", data.terrainSize},
            {"minHeight", data.terrainMinHeight},
            {"maxHeight", data.terrainMaxHeight},
            {"heights", data.terrainHeights}
        };
    }
    
    // Merge scene data with save data (scene data takes precedence for gameObjects)
    saveJson["gameObjects"] = sceneJson["gameObjects"];
    saveJson["name"] = sceneJson.value("name", data.sceneName);
    saveJson["isPaused"] = sceneJson.value("isPaused", false);

    // Save using SaveManager but with the merged JSON
    auto result = m_saveManager->QuickSaveWithJson(saveJson);
    if (!result.success) {
        gm::core::Logger::Warning("[Game] QuickSave failed: %s", result.message.c_str());
        if (m_tooling) m_tooling->AddNotification("QuickSave failed");
        gm::core::Event::Trigger(gotmilked::GameEvents::SceneSaveFailed);
    } else {
        gm::core::Logger::Info("[Game] QuickSave completed (with GameObjects)");
        if (m_tooling) m_tooling->AddNotification("QuickSave completed");
        gm::core::Event::Trigger(gotmilked::GameEvents::SceneQuickSaved);
    }
}

void Game::PerformQuickLoad() {
    if (!m_saveManager || !m_gameScene || !m_camera || !m_gameplay) {
        gm::core::Logger::Warning("[Game] QuickLoad unavailable (missing dependencies)");
        if (m_tooling) m_tooling->AddNotification("QuickLoad unavailable");
        return;
    }

    // Try loading with JSON first (new format with GameObjects)
    nlohmann::json saveJson;
    auto jsonResult = m_saveManager->QuickLoadWithJson(saveJson);
    
    if (jsonResult.success && saveJson.contains("gameObjects") && saveJson["gameObjects"].is_array()) {
        // New format with GameObjects - deserialize the scene
        std::string jsonString = saveJson.dump();
        if (!gm::SceneSerializer::Deserialize(*m_gameScene, jsonString)) {
            gm::core::Logger::Warning("[Game] QuickLoad failed to deserialize scene");
            if (m_tooling) m_tooling->AddNotification("QuickLoad failed");
            gm::core::Event::Trigger(gotmilked::GameEvents::SceneLoadFailed);
            return;
        }
        
        // Apply camera if present
        if (saveJson.contains("camera")) {
            const auto& camera = saveJson["camera"];
            if (camera.contains("position") && camera.contains("forward") && camera.contains("fov")) {
                auto pos = camera["position"];
                auto fwd = camera["forward"];
                if (pos.is_array() && pos.size() == 3 && fwd.is_array() && fwd.size() == 3) {
                    glm::vec3 cameraPos(pos[0].get<float>(), pos[1].get<float>(), pos[2].get<float>());
                    glm::vec3 cameraFwd(fwd[0].get<float>(), fwd[1].get<float>(), fwd[2].get<float>());
                    float cameraFov = camera.value("fov", 60.0f);
                    if (m_camera) {
                        m_camera->SetPosition(cameraPos);
                        m_camera->SetForward(cameraFwd);
                    }
                    if (m_gameplay) {
                        m_gameplay->SetFovDegrees(cameraFov);
                    }
                }
            }
        }
        
        // Apply world time if present
        if (saveJson.contains("worldTime") && m_gameplay) {
            double worldTime = saveJson.value("worldTime", 0.0);
            m_gameplay->SetWorldTimeSeconds(worldTime);
        }
        
        ApplyResourcesToScene();
        if (m_tooling) {
            m_tooling->AddNotification("QuickLoad applied (with GameObjects)");
        }
        gm::core::Event::Trigger(gotmilked::GameEvents::SceneQuickLoaded);
        return;
    }
    
    // Fall back to old format (no GameObjects)
    gm::save::SaveGameData data;
    auto result = m_saveManager->QuickLoad(data);
    if (!result.success) {
        gm::core::Logger::Warning("[Game] QuickLoad failed: %s", result.message.c_str());
        if (m_tooling) m_tooling->AddNotification("QuickLoad failed");
        gm::core::Event::Trigger(gotmilked::GameEvents::SceneLoadFailed);
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
    
    // Trigger event for successful load
    if (applied) {
        gm::core::Event::Trigger(gotmilked::GameEvents::SceneQuickLoaded);
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

