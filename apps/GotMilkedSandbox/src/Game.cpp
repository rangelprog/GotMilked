#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "Game.hpp"
#include "MeshSpinnerComponent.hpp"
#include "SandboxSceneHelpers.hpp"
#include "SceneSerializerExtensions.hpp"

#include <cstdio>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "gm/rendering/Camera.hpp"
#include "gm/scene/Transform.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/MaterialComponent.hpp"
#include "gm/scene/LightComponent.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/scene/SceneManager.hpp"
#include "gm/rendering/Material.hpp"
#include "gm/core/Input.hpp"
#include "gm/core/InputBindings.hpp"
#include "gm/core/input/InputManager.hpp"
#include "gm/core/Logger.hpp"
#include "save/SaveManager.hpp"
#include "gm/utils/ImGuiManager.hpp"
#include "tooling/ToolingOverlay.hpp"


Game::Game(const gm::utils::AppConfig& config)
    : m_config(config),
      m_assetsDir(config.paths.assets) {}

Game::~Game() = default;

bool Game::Init(GLFWwindow* window) {
    m_window = window;

    if (!m_resources.Load(m_assetsDir.string())) {
        return false;
    }

    gm::SceneSerializerExtensions::RegisterSerializers();

    m_camera = std::make_unique<gm::Camera>();

    // Set up input action bindings
    auto& inputManager = gm::core::InputManager::Instance();
    gm::core::InputBindings::SetupDefaultBindings(inputManager);

    SetupScene();
    ApplyResourcesToScene();

    m_gameplay = std::make_unique<sandbox::gameplay::SandboxGameplay>(*m_camera, m_resources, m_spinnerObjects, m_window);
    m_gameplay->SetScene(m_gameScene);

    m_saveManager = std::make_unique<sandbox::save::SaveManager>(m_config.paths.saves);

    m_imgui = std::make_unique<gm::utils::ImGuiManager>();
    if (m_imgui && !m_imgui->Init(m_window)) {
        gm::core::Logger::Warning("[Game] Failed to initialize ImGui; tooling overlay disabled");
        m_imgui.reset();
    }

    m_tooling = std::make_unique<sandbox::tooling::ToolingOverlay>();
    if (m_tooling) {
        sandbox::tooling::ToolingOverlay::Callbacks callbacks{
            [this]() { PerformQuickSave(); },
            [this]() { PerformQuickLoad(); },
            [this]() { ForceResourceReload(); }
        };
        m_tooling->SetCallbacks(std::move(callbacks));
        m_tooling->SetSaveManager(m_saveManager.get());
        m_tooling->SetHotReloader(&m_hotReloader);
        m_tooling->SetGameplay(m_gameplay.get());
        m_tooling->SetCamera(m_camera.get());
        m_tooling->SetScene(m_gameScene);
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

    sandbox::RehydrateMeshSpinnerComponents(*m_gameScene, m_resources, m_camera.get());
    sandbox::CollectMeshSpinnerObjects(*m_gameScene, m_spinnerObjects);

    if (m_spinnerObjects.empty()) {
        sandbox::PopulateSandboxScene(*m_gameScene, *m_camera, m_resources, m_spinnerObjects);
    } else {
        std::printf("[Game] Loaded %zu mesh spinner objects from scene\n", m_spinnerObjects.size());
    }

    if (m_gameplay) {
        m_gameplay->SetScene(m_gameScene);
    }
}

void Game::Update(float dt) {
    if (!m_window) return;

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
    if (!m_window || !m_resources.shader) return;

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

    if (m_gameScene) {
        sandbox::CollectMeshSpinnerObjects(*m_gameScene, m_spinnerObjects);
    } else {
        m_spinnerObjects.clear();
    }

    // Update projection matrix on all MeshSpinnerComponents
    for (auto& spinnerObject : m_spinnerObjects) {
        if (spinnerObject && spinnerObject->HasComponent<MeshSpinnerComponent>()) {
            if (auto spinner = spinnerObject->GetComponent<MeshSpinnerComponent>()) {
                spinner->SetProjectionMatrix(proj);
            }
        }
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
    // Clear spinner object references (they'll be destroyed with the scene)
    m_spinnerObjects.clear();
    
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
}

void Game::SetupResourceHotReload() {
    m_hotReloader.SetEnabled(m_config.hotReload.enable);
    m_hotReloader.SetPollInterval(m_config.hotReload.pollIntervalSeconds);

    if (!m_config.hotReload.enable) {
        return;
    }

    if (!m_resources.shaderVertPath.empty() && !m_resources.shaderFragPath.empty()) {
        m_hotReloader.AddWatch(
            "sandbox_shader",
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
            "sandbox_texture",
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
            "sandbox_mesh",
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

    sandbox::RehydrateMeshSpinnerComponents(*m_gameScene, m_resources, m_camera.get());
    sandbox::CollectMeshSpinnerObjects(*m_gameScene, m_spinnerObjects);

    for (auto& obj : m_spinnerObjects) {
        if (!obj) {
            continue;
        }

        if (auto materialComp = obj->GetComponent<gm::MaterialComponent>()) {
            if (auto mat = materialComp->GetMaterial()) {
                mat->SetDiffuseTexture(m_resources.texture ? m_resources.texture.get() : nullptr);
            }
        }
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

    auto data = sandbox::save::CaptureSnapshot(*m_gameScene, *m_camera, *m_gameplay);
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

    sandbox::save::SaveGameData data;
    auto result = m_saveManager->QuickLoad(data);
    if (!result.success) {
        gm::core::Logger::Warning("[Game] QuickLoad failed: %s", result.message.c_str());
        if (m_tooling) m_tooling->AddNotification("QuickLoad failed");
        return;
    }

    sandbox::save::ApplySnapshot(data, *m_gameScene, *m_camera, *m_gameplay);
    ApplyResourcesToScene();
    if (m_tooling) m_tooling->AddNotification("QuickLoad applied");
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
