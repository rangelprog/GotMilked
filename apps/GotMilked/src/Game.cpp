#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "Game.hpp"
#include "GameSceneHelpers.hpp"
#include "SceneSerializerExtensions.hpp"
#include "StaticMeshComponent.hpp"
#include "gm/physics/PhysicsWorld.hpp"

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
        m_tooling->AddNotification("Tooling overlay ready (F1)");
    }

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
        gotmilked::PopulateInitialScene(*m_gameScene, *m_camera, m_resources);
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
        if (m_imgui && m_imgui->IsInitialized()) {
            m_overlayVisible = !m_overlayVisible;
            if (m_tooling) {
                m_tooling->AddNotification(m_overlayVisible ? "Tooling overlay shown" : "Tooling overlay hidden");
            }
        } else {
            gm::core::Logger::Warning("[Game] ImGui not initialized; overlay not available");
        }
    }

    if (m_gameplay) {
        m_gameplay->SetWindow(m_window);
        m_gameplay->SetScene(m_gameScene);
        m_gameplay->SetInputSuppressed(m_overlayVisible);
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
        return;
    }

    auto applyToObject = [this](const std::string& name,
                                gm::Mesh* mesh,
                                const std::shared_ptr<gm::Material>& material) {
        if (!m_gameScene || !mesh || !material || !m_resources.shader) {
            return;
        }

        auto object = m_gameScene->FindGameObjectByName(name);
        if (!object) {
            return;
        }

        auto renderer = object->GetComponent<StaticMeshComponent>();
        if (!renderer) {
            return;
        }

        renderer->SetMesh(mesh);
        renderer->SetShader(m_resources.shader.get());
        renderer->SetMaterial(material);
        renderer->SetCamera(m_camera.get());
    };

    applyToObject("GroundPlane", m_resources.planeMesh.get(), m_resources.planeMaterial);
    applyToObject("FloatingCube", m_resources.cubeMesh.get(), m_resources.cubeMaterial);

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

