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
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include "Game.hpp"
#include "GameBootstrapper.hpp"
#include "GameRenderer.hpp"
#include "DebugToolingController.hpp"
#include "SceneResourceController.hpp"
#include "GameShutdownController.hpp"
#include "ToolingFacade.hpp"
#include "EventRouter.hpp"
#include "GameSceneHelpers.hpp"
#include "GameConstants.hpp"
#include "GameEvents.hpp"
#include "SceneSerializerExtensions.hpp"
#include "GameLoopController.hpp"
#include "gm/gameplay/FlyCameraController.hpp"
#include "gm/scene/SceneSerializer.hpp"
#include "gm/scene/PrefabLibrary.hpp"
#include "gm/utils/Profiler.hpp"

#include <filesystem>
#include <optional>
#include <algorithm>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <nlohmann/json.hpp>
#include <typeinfo>
#include <fmt/format.h>

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
#include "gm/save/SaveDiff.hpp"
#include "gm/save/SaveVersion.hpp"
#include "gm/utils/ImGuiManager.hpp"
#include "gm/utils/HotReloader.hpp"
#include "gm/tooling/Overlay.hpp"
#include "gm/gameplay/CameraRigSystem.hpp"
#include "gm/gameplay/CameraRigComponent.hpp"
#include "gm/gameplay/QuestTriggerComponent.hpp"
#include "gm/gameplay/QuestTriggerSystem.hpp"
#if GM_DEBUG_TOOLS
#include "EditableTerrainComponent.hpp"
#include "DebugMenu.hpp"
#include "gm/tooling/DebugConsole.hpp"
#include "DebugHudController.hpp"
#include "gm/debug/GridRenderer.hpp"

using gm::debug::EditableTerrainComponent;
#endif
#include <imgui.h>
#include <glad/glad.h>


Game::Game(const gm::utils::AppConfig& config)
    : m_config(config),
      m_assetsDir(config.paths.assets) {
#if GM_DEBUG_TOOLS
    m_debugHud = std::make_unique<gm::debug::DebugHudController>();
    m_terrainEditingSystem = std::make_shared<gm::debug::TerrainEditingSystem>();
#endif
    m_bootstrapper = std::make_unique<GameBootstrapper>(*this);
    m_renderer = std::make_unique<GameRenderer>(*this);
    m_toolingFacade = std::make_unique<ToolingFacade>(*this);
    m_cameraRigSystem = std::make_shared<gm::gameplay::CameraRigSystem>();
    m_questSystem = std::make_shared<gm::gameplay::QuestTriggerSystem>();
    m_resources.SetIssueReporter([this](const std::string& message, bool isError) {
        if (!m_toolingFacade) {
            return;
        }
        const std::string formatted = isError
            ? fmt::format("Resource error: {}", message)
            : fmt::format("Resource warning: {}", message);
        m_toolingFacade->AddNotification(formatted);
    });
    m_debugTooling = std::make_unique<DebugToolingController>(*this);
    m_sceneResources = std::make_unique<SceneResourceController>(*this);
    m_shutdownController = std::make_unique<GameShutdownController>(*this);
    m_eventRouter = std::make_unique<EventRouter>();
    m_loopController = std::make_unique<GameLoopController>(*this);
}

Game::~Game() = default;

bool Game::Init(GLFWwindow* window, gm::SceneManager& sceneManager) {
    if (!m_bootstrapper) {
        m_bootstrapper = std::make_unique<GameBootstrapper>(*this);
    }
    if (!m_debugTooling) {
        m_debugTooling = std::make_unique<DebugToolingController>(*this);
    }
    return m_bootstrapper->Initialize(window, sceneManager);
}

bool Game::SetupLogging() {
    // Use user documents directory for logs (same parent as saves)
    std::filesystem::path logDir;
    std::filesystem::path userDocsPath = gm::utils::ConfigLoader::GetUserDocumentsPath();
    if (!userDocsPath.empty()) {
        logDir = userDocsPath / "logs";
    } else {
        // Fallback to saves directory if user docs unavailable
        logDir = m_config.paths.saves / "logs";
    }
    
    std::error_code ec;
    std::filesystem::create_directories(logDir, ec);
    if (ec) {
        gm::core::Logger::Error("[Game] Failed to create log directory '{}': {}", logDir.string(), ec.message());
        return false;
    }
    
    const std::filesystem::path logPath = logDir / "game.log";
    gm::core::Logger::SetLogFile(logPath);
    gm::core::Logger::Info("[Game] Logging to {}", logPath.string());
#ifdef GM_DEBUG
    gm::core::Logger::SetDebugEnabled(true);
#endif
    return true;
}

bool Game::SetupPhysics() {
    auto& physics = gm::physics::PhysicsWorld::Instance();
    if (!physics.IsInitialized()) {
        try {
            physics.Init();
        } catch (const std::exception& ex) {
            gm::core::Logger::Error("[Game] Failed to initialize physics: {}", ex.what());
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
    if (!m_resources.Load(m_assetsDir)) {
        gm::core::Logger::Error("[Game] Failed to load resources from {}", m_assetsDir.string());
        return false;
    }
    m_assetsDir = m_resources.GetAssetsDirectory();

    gm::SceneSerializerExtensions::RegisterSerializers();
    m_camera = std::make_unique<gm::Camera>();
    return true;
}

void Game::SetupInput() {
    auto& inputManager = gm::core::InputManager::Instance();
    gm::core::InputBindings::SetupDefaultBindings(inputManager);
}

void Game::SetupGameplay() {
    if (!m_cameraRigSystem) {
        m_cameraRigSystem = std::make_shared<gm::gameplay::CameraRigSystem>();
    }
    m_cameraRigSystem->SetActiveCamera(m_camera.get());
    m_cameraRigSystem->SetWindow(m_window);
    m_cameraRigSystem->SetSceneContext(m_gameScene);

#if GM_DEBUG_TOOLS
    if (m_terrainEditingSystem) {
        m_terrainEditingSystem->SetCamera(GetRenderCamera());
        m_terrainEditingSystem->SetWindow(m_window);
        m_terrainEditingSystem->SetFovProvider([this]() -> float {
            return m_cameraRigSystem ? m_cameraRigSystem->GetFovDegrees()
                                     : gotmilked::GameConstants::Camera::DefaultFovDegrees;
        });
        m_terrainEditingSystem->SetSceneContext(m_gameScene);
    }
#endif

    if (m_questSystem) {
        m_questSystem->SetSceneContext(m_gameScene);
        m_questSystem->SetPlayerPositionProvider([this]() -> glm::vec3 {
            return m_camera ? m_camera->Position() : glm::vec3(0.0f);
        });
        m_questSystem->SetTriggerCallback([this](const gm::gameplay::QuestTriggerComponent& trigger) {
            const std::string questId = trigger.GetQuestId();
            if (questId.empty()) {
                return;
            }
            const bool firstTrigger = m_completedQuests.insert(questId).second;
            const std::string message = firstTrigger
                ? fmt::format("Quest triggered: {}", questId)
                : fmt::format("Quest updated: {}", questId);
            gm::core::Logger::Info("[Game] {}", message);
            if (m_toolingFacade) {
                m_toolingFacade->AddNotification(message);
            }
        });
    }
}

void Game::SetupSaveSystem() {
    m_saveManager = std::make_unique<gm::save::SaveManager>(m_config.paths.saves);
}


namespace {
constexpr const char* kStarterSceneFilename = "starter.scene.json";
}

void Game::SetupScene() {
    if (!m_sceneManager) {
        gm::core::Logger::Error("[Game] No SceneManager provided");
        return;
    }

    m_gameScene = m_sceneManager->LoadScene("GameScene");
    if (!m_gameScene) {
        gm::core::Logger::Error("[Game] Failed to create game scene");
        return;
    }

    if (m_cameraRigSystem) {
        m_cameraRigSystem->SetSceneContext(m_gameScene);
        m_gameScene->RegisterSystem(m_cameraRigSystem);
    }
#if GM_DEBUG_TOOLS
    if (m_terrainEditingSystem) {
        m_terrainEditingSystem->SetSceneContext(m_gameScene);
        m_gameScene->RegisterSystem(m_terrainEditingSystem);
    }
#endif
    if (m_questSystem) {
        m_questSystem->SetSceneContext(m_gameScene);
        m_gameScene->RegisterSystem(m_questSystem);
    }

    EnsureCameraRig();

    std::filesystem::path starterRoot = m_config.paths.saves;
    if (starterRoot.empty()) {
        starterRoot = m_assetsDir / "saves";
    }
    std::error_code canonicalSavesEc;
    auto canonicalSaves = std::filesystem::weakly_canonical(starterRoot, canonicalSavesEc);
    if (canonicalSavesEc) {
        canonicalSaves = std::filesystem::absolute(starterRoot);
    }
    const auto starterScenePath = (canonicalSaves / kStarterSceneFilename).lexically_normal();

    bool loadedFromDisk = false;
    std::error_code existsEc;
    if (std::filesystem::exists(starterScenePath, existsEc)) {
        gm::core::Logger::Info("[Game] Loading starter scene from '{}'", starterScenePath.string());
        if (gm::SceneSerializer::LoadFromFile(*m_gameScene, starterScenePath.string())) {
            loadedFromDisk = true;
        } else {
            gm::core::Logger::Warning("[Game] Failed to load starter scene from '{}'; rebuilding default scene", starterScenePath.string());
            m_gameScene->Cleanup();
        }
    } else if (existsEc) {
        gm::core::Logger::Warning("[Game] Could not check starter scene at '{}': {}", starterScenePath.string(), existsEc.message());
    }

    m_gameScene->SetParallelGameObjectUpdates(true);

    if (loadedFromDisk && m_gameScene->GetAllGameObjects().empty()) {
        gm::core::Logger::Warning("[Game] Starter scene file '{}' was empty; rebuilding default scene", starterScenePath.string());
        loadedFromDisk = false;
    }

    if (!loadedFromDisk) {
        m_gameScene->Cleanup();

        auto fovProvider = [this]() -> float {
        return m_cameraRigSystem ? m_cameraRigSystem->GetFovDegrees() : 60.0f;
        };
        gotmilked::PopulateInitialScene(*m_gameScene, *m_camera, m_resources, m_window, fovProvider);

        std::error_code dirEc;
        std::filesystem::create_directories(canonicalSaves, dirEc);
        if (dirEc) {
            gm::core::Logger::Warning("[Game] Failed to create saves directory '{}': {}", canonicalSaves.string(), dirEc.message());
        } else if (gm::SceneSerializer::SaveToFile(*m_gameScene, starterScenePath.string())) {
            gm::core::Logger::Info("[Game] Generated starter scene at '{}'", starterScenePath.string());
        } else {
            gm::core::Logger::Warning("[Game] Failed to save generated starter scene to '{}'", starterScenePath.string());
        }
    } else {
        gm::core::Logger::Info("[Game] Starter scene loaded successfully");
    }

    gm::core::Logger::Info("[Game] Game scene initialized successfully");

    ApplyResourcesToScene();

    if (m_cameraRigSystem) {
        m_cameraRigSystem->SetSceneContext(m_gameScene);
    }
}

void Game::Update(float dt) {
    gm::utils::Profiler::Instance().BeginFrame();
    if (m_loopController) {
        m_loopController->Update(dt);
    }
}


void Game::Render() {
    if (m_renderer) {
        m_renderer->Render();
    }
    gm::utils::Profiler::Instance().EndFrame();
}

void Game::Shutdown() {
    SetDebugViewportCameraActive(false);
    if (m_shutdownController) {
        m_shutdownController->Shutdown();
    }
}

void Game::SetupEventSubscriptions() {
    if (!m_eventRouter) {
        m_eventRouter = std::make_unique<EventRouter>();
    }
    m_eventRouter->Clear();

    auto notify = [this](const char* message) {
        if (m_toolingFacade) {
            m_toolingFacade->AddNotification(message);
        }
    };

    auto refreshHud = [this]() {
        if (m_toolingFacade) {
            m_toolingFacade->RefreshHud();
        }
    };

    const struct {
        const char* name;
        gm::core::Event::EventCallback callback;
    } handlers[] = {
        {gotmilked::GameEvents::ResourceShaderLoaded, [this, notify, refreshHud]() {
             gm::core::Logger::Debug("[Game] Event: Shader loaded");
             notify("Shader loaded");
             refreshHud();
         }},
        {gotmilked::GameEvents::ResourceShaderReloaded, [this, notify, refreshHud]() {
             gm::core::Logger::Debug("[Game] Event: Shader reloaded");
             notify("Shader reloaded");
             refreshHud();
         }},
        {gotmilked::GameEvents::ResourceTextureLoaded, [this, refreshHud]() {
             gm::core::Logger::Debug("[Game] Event: Texture loaded");
             refreshHud();
         }},
        {gotmilked::GameEvents::ResourceTextureReloaded, [this, notify, refreshHud]() {
             gm::core::Logger::Debug("[Game] Event: Texture reloaded");
             notify("Texture reloaded");
             refreshHud();
         }},
        {gotmilked::GameEvents::ResourceMeshLoaded, [this, refreshHud]() {
             gm::core::Logger::Debug("[Game] Event: Mesh loaded");
             refreshHud();
         }},
        {gotmilked::GameEvents::ResourceMeshReloaded, [this, notify, refreshHud]() {
             gm::core::Logger::Debug("[Game] Event: Mesh reloaded");
             notify("Mesh reloaded");
             refreshHud();
         }},
        {gotmilked::GameEvents::ResourceAllReloaded, [this, notify, refreshHud]() {
             gm::core::Logger::Info("[Game] Event: All resources reloaded");
             notify("All resources reloaded");
             refreshHud();
         }},
        {gotmilked::GameEvents::ResourceLoadFailed, [this, notify]() {
             gm::core::Logger::Warning("[Game] Event: Resource load failed");
             notify("Resource load failed");
         }},
        {gotmilked::GameEvents::SceneQuickSaved, [this]() {
             gm::core::Logger::Debug("[Game] Event: Scene quick saved");
         }},
        {gotmilked::GameEvents::SceneQuickLoaded, [this]() {
             gm::core::Logger::Debug("[Game] Event: Scene quick loaded");
         }},
        {gotmilked::GameEvents::GameInitialized, [this]() {
             gm::core::Logger::Info("[Game] Event: Game initialized");
         }},
        {gotmilked::GameEvents::GameShutdown, [this]() {
             gm::core::Logger::Info("[Game] Event: Game shutdown");
         }},
    };

    for (const auto& handler : handlers) {
        m_eventRouter->Register(handler.name, handler.callback);
    }
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
                const std::string shaderGuid = m_resources.GetShaderGuid();
                bool ok = m_resources.ReloadShader(shaderGuid);
                if (ok) {
                    if (m_sceneResources) {
                        m_sceneResources->RefreshShaders({shaderGuid});
                    }
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
                const std::string meshGuid = m_resources.GetMeshPath().empty() ? std::string() : m_resources.GetMeshGuid();
                bool ok = meshGuid.empty() ? m_resources.ReloadMesh() : m_resources.ReloadMesh(meshGuid);
                if (ok) {
                    if (m_sceneResources) {
                        if (!meshGuid.empty()) {
                            m_sceneResources->RefreshMeshes({meshGuid});
                        } else {
                            m_sceneResources->ApplyResourcesToStaticMeshComponents();
                        }
                    }
                    gm::core::Event::Trigger(gotmilked::GameEvents::HotReloadMeshReloaded);
                }
                return ok;
            });
    }

    m_hotReloader.ForcePoll();
}

void Game::ApplyResourcesToScene() {
    if (m_sceneResources) {
        m_sceneResources->ApplyResourcesToScene();
    }
    EnsureCameraRig();
    if (m_tooling) {
        m_tooling->SetCamera(GetRenderCamera());
    }
    if (m_questSystem) {
        m_questSystem->SetSceneContext(m_gameScene);
    }
#if GM_DEBUG_TOOLS
    if (m_terrainEditingSystem) {
        m_terrainEditingSystem->SetCamera(GetRenderCamera());
        m_terrainEditingSystem->RefreshBindings();
    }
#endif
}

void Game::ApplyResourcesToStaticMeshComponents() {
    if (m_sceneResources) {
        m_sceneResources->ApplyResourcesToStaticMeshComponents();
    }
}

#if GM_DEBUG_TOOLS
void Game::ApplyResourcesToTerrain() {
    if (m_sceneResources) {
        m_sceneResources->ApplyResourcesToTerrain();
    }
    if (m_terrainEditingSystem) {
        m_terrainEditingSystem->RefreshBindings();
    }
}

#endif

void Game::EnsureCameraRig() {
    if (!m_gameScene) {
        return;
    }

    std::shared_ptr<gm::GameObject> cameraRigObject;
    for (const auto& obj : m_gameScene->GetAllGameObjects()) {
        if (!obj) {
            continue;
        }
        if (obj->GetComponent<gm::gameplay::CameraRigComponent>()) {
            // A valid camera rig already exists
            return;
        }

        if (!cameraRigObject && obj->GetName() == "CameraRig") {
            cameraRigObject = obj;
        }
    }

    bool spawnedNewObject = false;
    if (!cameraRigObject) {
        cameraRigObject = m_gameScene->SpawnGameObject("CameraRig");
        if (!cameraRigObject) {
            gm::core::Logger::Warning("[Game] Failed to spawn CameraRig GameObject");
            return;
        }
        spawnedNewObject = true;
    }

    auto rig = cameraRigObject->GetComponent<gm::gameplay::CameraRigComponent>();
    if (!rig) {
        rig = cameraRigObject->AddComponent<gm::gameplay::CameraRigComponent>();
        if (!rig) {
            gm::core::Logger::Warning("[Game] Failed to add CameraRigComponent to CameraRig GameObject");
            return;
        }
        rig->SetRigId("PrimaryCamera");
        rig->SetInitialFov(gotmilked::GameConstants::Camera::DefaultFovDegrees);
    } else if (spawnedNewObject) {
        // Newly spawned object already had a rig component (unlikely), but ensure defaults
        rig->SetRigId("PrimaryCamera");
        rig->SetInitialFov(gotmilked::GameConstants::Camera::DefaultFovDegrees);
    }
}

gm::Camera* Game::GetRenderCamera() const {
#if GM_DEBUG_TOOLS
    if (m_viewportCameraActive && m_viewportCamera) {
        return m_viewportCamera.get();
    }
#endif
    return m_camera.get();
}

float Game::GetRenderCameraFov() const {
#if GM_DEBUG_TOOLS
    if (m_viewportCameraActive && m_viewportCameraController) {
        return m_viewportCameraController->GetFovDegrees();
    }
#endif
    return m_cameraRigSystem ? m_cameraRigSystem->GetFovDegrees()
                             : gotmilked::GameConstants::Camera::DefaultFovDegrees;
}

void Game::SetDebugViewportCameraActive(bool enabled) {
#if GM_DEBUG_TOOLS
    if (enabled == m_viewportCameraActive) {
        return;
    }

    if (enabled) {
        if (!m_viewportCamera) {
            m_viewportCamera = std::make_unique<gm::Camera>();
        }
        if (m_camera) {
            m_viewportSavedPosition = m_camera->Position();
            m_viewportSavedForward = m_camera->Front();
            m_viewportSavedFov = m_cameraRigSystem ? m_cameraRigSystem->GetFovDegrees()
                                                   : gotmilked::GameConstants::Camera::DefaultFovDegrees;
            m_viewportCameraHasSavedPose = true;
        }
        m_viewportCamera->SetPosition(m_viewportSavedPosition);
        m_viewportCamera->SetForward(m_viewportSavedForward);
        m_viewportCamera->SetFov(m_viewportSavedFov);

        gm::gameplay::FlyCameraController::Config config;
        config.initialFov = m_viewportSavedFov;
        config.fovMin = 30.0f;
        config.fovMax = 100.0f;
        config.fovScrollSensitivity = 2.0f;
        m_viewportCameraController = std::make_unique<gm::gameplay::FlyCameraController>(
            *m_viewportCamera,
            m_window,
            config);
        if (m_gameScene) {
            m_viewportCameraController->SetScene(m_gameScene);
        }
        m_viewportCameraActive = true;

        if (m_tooling) {
            m_tooling->SetCamera(m_viewportCamera.get());
        }
        if (m_terrainEditingSystem) {
            m_terrainEditingSystem->SetCamera(m_viewportCamera.get());
        }
    } else {
        if (m_viewportCameraController && m_viewportCamera) {
            m_viewportSavedPosition = m_viewportCamera->Position();
            m_viewportSavedForward = m_viewportCamera->Front();
            m_viewportSavedFov = m_viewportCameraController->GetFovDegrees();
        }
        if (m_tooling) {
            m_tooling->SetCamera(m_camera.get());
        }
        if (m_terrainEditingSystem) {
            m_terrainEditingSystem->SetCamera(m_camera.get());
        }
        m_viewportCameraController.reset();
        m_viewportCamera.reset();
        m_viewportCameraHasSavedPose = false;
        m_viewportCameraActive = false;
    }
#else
    (void)enabled;
#endif
}

bool Game::IsDebugViewportCameraActive() const {
#if GM_DEBUG_TOOLS
    return m_viewportCameraActive;
#else
    return false;
#endif
}

void Game::UpdateViewportCamera(float deltaTime, bool inputSuppressed) {
#if GM_DEBUG_TOOLS
    if (!m_viewportCameraActive || !m_viewportCameraController) {
        return;
    }
    m_viewportCameraController->SetWindow(m_window);
    if (m_gameScene) {
        m_viewportCameraController->SetScene(m_gameScene);
    }
    m_viewportCameraController->SetInputSuppressed(inputSuppressed);
    m_viewportCameraController->Update(deltaTime);
    if (m_viewportCamera) {
        m_viewportSavedPosition = m_viewportCamera->Position();
        m_viewportSavedForward = m_viewportCamera->Front();
        m_viewportSavedFov = m_viewportCameraController->GetFovDegrees();
        m_viewportCameraHasSavedPose = true;
    }
#else
    (void)deltaTime;
    (void)inputSuppressed;
#endif
}

void Game::PerformQuickSave() {
    if (!m_saveManager || !m_gameScene || !m_camera || !m_cameraRigSystem) {
        gm::core::Logger::Warning("[Game] QuickSave unavailable (missing dependencies)");
        if (m_toolingFacade) m_toolingFacade->AddNotification("QuickSave unavailable");
        return;
    }

    auto data = gm::save::SaveSnapshotHelpers::CaptureSnapshot(
        m_camera.get(),
        m_gameScene,
        [this]() { return m_cameraRigSystem ? m_cameraRigSystem->GetWorldTimeSeconds() : 0.0; });
    
    // Add FOV to save data
    data.cameraFov = m_cameraRigSystem->GetFovDegrees();

#if GM_DEBUG_TOOLS
    if (m_gameScene) {
        auto terrainObject = m_gameScene->FindGameObjectByName("Terrain");
        if (terrainObject) {
            if (auto terrain = terrainObject->GetComponent<EditableTerrainComponent>()) {
                data.terrainResolution = terrain->GetResolution();
                data.terrainSize = terrain->GetTerrainSize();
                data.terrainMinHeight = terrain->GetMinHeight();
                data.terrainMaxHeight = terrain->GetMaxHeight();
                data.terrainHeights = terrain->GetHeights();
                data.terrainTextureTiling = terrain->GetTextureTiling();
                data.terrainBaseTextureGuid = terrain->GetBaseTextureGuid();
                data.terrainActivePaintLayer = terrain->GetActivePaintLayerIndex();
                data.terrainPaintLayers.clear();
                const int paintLayerCount = terrain->GetPaintLayerCount();
                data.terrainPaintLayers.reserve(paintLayerCount);
                for (int layer = 0; layer < paintLayerCount; ++layer) {
                    gm::save::SaveGameData::TerrainPaintLayerData layerData;
                    layerData.guid = terrain->GetPaintTextureGuid(layer);
                    layerData.enabled = terrain->IsPaintLayerEnabled(layer);
                    const auto& weights = terrain->GetPaintLayerWeights(layer);
                    layerData.weights.assign(weights.begin(), weights.end());
                    data.terrainPaintLayers.push_back(std::move(layerData));
                }
            }
        }
    }
#endif

    // Serialize the scene to include all GameObjects and their properties
    std::string sceneJsonString = gm::SceneSerializer::Serialize(*m_gameScene);
    nlohmann::json sceneJson = nlohmann::json::parse(sceneJsonString);
    
    // Merge SaveGameData into the scene JSON
    nlohmann::json saveJson = {
        {"version", gm::save::SaveVersionToJson(data.version)},
        {"sceneName", data.sceneName},
        {"camera", {
            {"position", {data.cameraPosition.x, data.cameraPosition.y, data.cameraPosition.z}},
            {"forward", {data.cameraForward.x, data.cameraForward.y, data.cameraForward.z}},
            {"fov", data.cameraFov}
        }},
        {"worldTime", data.worldTime}
    };
    
    if (data.terrainResolution > 0 && !data.terrainHeights.empty()) {
        nlohmann::json terrainJson = {
            {"resolution", data.terrainResolution},
            {"size", data.terrainSize},
            {"minHeight", data.terrainMinHeight},
            {"maxHeight", data.terrainMaxHeight},
            {"heights", data.terrainHeights},
            {"textureTiling", data.terrainTextureTiling},
            {"baseTextureGuid", data.terrainBaseTextureGuid},
            {"activePaintLayer", data.terrainActivePaintLayer}
        };

        nlohmann::json paintLayers = nlohmann::json::array();
        for (const auto& layer : data.terrainPaintLayers) {
            nlohmann::json layerJson;
            layerJson["guid"] = layer.guid;
            layerJson["enabled"] = layer.enabled;
            layerJson["weights"] = layer.weights;
            paintLayers.push_back(std::move(layerJson));
        }
        terrainJson["paintLayers"] = std::move(paintLayers);

        saveJson["terrain"] = std::move(terrainJson);
    }
    
    // Merge scene data with save data (scene data takes precedence for gameObjects)
    saveJson["gameObjects"] = sceneJson["gameObjects"];
    saveJson["name"] = sceneJson.value("name", data.sceneName);
    saveJson["isPaused"] = sceneJson.value("isPaused", false);

    nlohmann::json metadata;
    metadata["runtimeVersion"] = gm::save::SaveVersionToJson(gm::save::SaveVersion::Current());
    metadata["versionString"] = data.version.ToString();

    bool terrainFallbackApplied = false;
    if (m_saveManager) {
        nlohmann::json previousJson;
        auto previousResult = m_saveManager->LoadMostRecentQuickSaveJson(previousJson);
        if (previousResult.success) {
            if (!saveJson.contains("terrain")) {
                gm::save::MergeTerrainIfMissing(saveJson, previousJson);
                terrainFallbackApplied = saveJson.contains("terrain");
            }

            auto diffSummary = gm::save::ComputeSaveDiff(previousJson, saveJson);
            nlohmann::json diffJson;
            diffJson["versionChanged"] = diffSummary.versionChanged;
            diffJson["terrainChanged"] = diffSummary.terrainChanged;
            diffJson["questStateChanged"] = diffSummary.questStateChanged;
            diffJson["terrainFallbackApplied"] = terrainFallbackApplied;
            if (!diffSummary.terrainDiff.is_null()) {
                diffJson["terrainDiff"] = diffSummary.terrainDiff;
            }
            if (diffSummary.questStateChanged) {
                diffJson["questChanges"] = diffSummary.questChanges;
                for (const auto& change : diffSummary.questChanges) {
                    gm::core::Logger::Info("[Game] Quest diff: {}", change);
                }
            }
            if (diffSummary.terrainChanged) {
                gm::core::Logger::Info("[Game] Terrain data changed since last quick save");
            }
            if (diffSummary.versionChanged) {
                gm::core::Logger::Info("[Game] Save version updated to {}", data.version.ToString());
            }
            metadata["diff"] = std::move(diffJson);
        } else if (previousResult.message != "No quick save found") {
            gm::core::Logger::Warning("[Game] Unable to load previous quick save for diff: {}", previousResult.message);
        }
    }

    if (terrainFallbackApplied) {
        gm::core::Logger::Info("[Game] Applied terrain data fallback from previous quick save");
    }

    saveJson["metadata"] = std::move(metadata);

    // Save using SaveManager but with the merged JSON
    auto result = m_saveManager->QuickSaveWithJson(saveJson);
    if (!result.success) {
        gm::core::Logger::Warning("[Game] QuickSave failed: {}", result.message);
        if (m_toolingFacade) m_toolingFacade->AddNotification("QuickSave failed");
        gm::core::Event::Trigger(gotmilked::GameEvents::SceneSaveFailed);
    } else {
        gm::core::Logger::Info("[Game] QuickSave completed (with GameObjects)");
        if (m_toolingFacade) m_toolingFacade->AddNotification("QuickSave completed");
        gm::core::Event::Trigger(gotmilked::GameEvents::SceneQuickSaved);
    }
}

void Game::PerformQuickLoad() {
    if (!m_saveManager || !m_gameScene || !m_camera || !m_cameraRigSystem) {
        gm::core::Logger::Warning("[Game] QuickLoad unavailable (missing dependencies)");
        if (m_toolingFacade) m_toolingFacade->AddNotification("QuickLoad unavailable");
        return;
    }

    // Try loading with JSON first (new format with GameObjects)
    nlohmann::json saveJson;
    auto jsonResult = m_saveManager->QuickLoadWithJson(saveJson);
    
    if (jsonResult.success && saveJson.contains("gameObjects") && saveJson["gameObjects"].is_array()) {
        gm::save::SaveVersion fileVersion = gm::save::SaveVersion::Current();
        if (saveJson.contains("version")) {
            fileVersion = gm::save::ParseSaveVersion(saveJson["version"]);
        } else {
            gm::core::Logger::Warning("[Game] QuickLoad: save is missing version information; assuming current");
        }
        const auto runtimeVersion = gm::save::SaveVersion::Current();
        if (!fileVersion.IsCompatibleWith(runtimeVersion)) {
            gm::core::Logger::Warning(
                "[Game] QuickLoad: save version {} is not fully compatible with runtime {}; attempting migration",
                fileVersion.ToString(), runtimeVersion.ToString());
        }

        // New format with GameObjects - deserialize the scene
        std::string jsonString = saveJson.dump();
#if GM_DEBUG_TOOLS
        if (m_debugMenu) {
            m_debugMenu->BeginSceneReload();
        }
#endif
        bool ok = gm::SceneSerializer::Deserialize(*m_gameScene, jsonString);
        gm::core::Logger::Info("[Game] QuickLoad JSON deserialize result: {}", ok ? "success" : "failure");
#if GM_DEBUG_TOOLS
        if (m_debugMenu) {
            m_debugMenu->EndSceneReload();
        }
#endif
        if (!ok) {
            gm::core::Logger::Warning("[Game] QuickLoad failed to deserialize scene");
            if (m_toolingFacade) m_toolingFacade->AddNotification("QuickLoad failed");
            gm::core::Event::Trigger(gotmilked::GameEvents::SceneLoadFailed);
            return;
        }

        if (m_gameScene) {
            m_gameScene->BumpReloadVersion();
        }

        auto quickObjects = m_gameScene->GetAllGameObjects();
        gm::core::Logger::Info("[Game] QuickLoad scene object count: {}", quickObjects.size());
        for (const auto& obj : quickObjects) {
            if (!obj) {
                gm::core::Logger::Error("[Game] QuickLoad: encountered null GameObject");
                continue;
            }
            gm::core::Logger::Info("[Game] QuickLoad: raw name '{}'", obj->GetName());
            const std::string& objName = obj->GetName();
            if (objName.empty()) {
                gm::core::Logger::Error("[Game] QuickLoad: GameObject with empty name (ptr {})", static_cast<const void*>(obj.get()));
            } else {
                gm::core::Logger::Info("[Game] QuickLoad: GameObject '{}'", objName);
            }

            auto comps = obj->GetComponents();
            gm::core::Logger::Info("[Game] QuickLoad: '{}' has {} components", objName.c_str(), comps.size());
            for (const auto& comp : comps) {
                if (!comp) {
                    gm::core::Logger::Error("[Game] QuickLoad: '{}' has null component", objName.c_str());
                    continue;
                }
                const std::string& compName = comp->GetName();
                if (compName.empty()) {
                    gm::core::Logger::Error("[Game] QuickLoad: component with empty name on '{}' (typeid {})",
                        objName.c_str(), typeid(*comp).name());
                } else {
                    gm::core::Logger::Info("[Game] QuickLoad:     Component '{}'", compName);
                }
            }
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
            if (m_cameraRigSystem) {
                m_cameraRigSystem->SetFovDegrees(cameraFov);
                    }
                }
            }
        }
        
        // Apply world time if present
        if (saveJson.contains("worldTime") && m_cameraRigSystem) {
            double worldTime = saveJson.value("worldTime", 0.0);
            m_cameraRigSystem->SetWorldTimeSeconds(worldTime);
        }
        
        ApplyResourcesToScene();
        if (m_cameraRigSystem) {
            m_cameraRigSystem->SetSceneContext(m_gameScene);
        }
#if GM_DEBUG_TOOLS
        if (m_terrainEditingSystem) {
            m_terrainEditingSystem->SetSceneContext(m_gameScene);
        }
#endif
        if (m_questSystem) {
            m_questSystem->SetSceneContext(m_gameScene);
        }
        if (m_toolingFacade) {
            m_toolingFacade->AddNotification("QuickLoad applied (with GameObjects)");
        }
        gm::core::Event::Trigger(gotmilked::GameEvents::SceneQuickLoaded);
        return;
    }
    
    // Fall back to old format (no GameObjects)
    gm::save::SaveGameData data;
    auto result = m_saveManager->QuickLoad(data);
    if (!result.success) {
        gm::core::Logger::Warning("[Game] QuickLoad failed: {}", result.message);
        if (m_toolingFacade) m_toolingFacade->AddNotification("QuickLoad failed");
        gm::core::Event::Trigger(gotmilked::GameEvents::SceneLoadFailed);
        return;
    }

    if (!data.version.IsCompatibleWith(gm::save::SaveVersion::Current())) {
        gm::core::Logger::Warning(
            "[Game] QuickLoad: legacy save version {} may be incompatible with runtime {}; attempting migration",
            data.version.ToString(), gm::save::SaveVersion::Current().ToString());
    }

#if GM_DEBUG_TOOLS
    if (m_debugMenu) {
        m_debugMenu->BeginSceneReload();
    }
#endif

    bool applied = gm::save::SaveSnapshotHelpers::ApplySnapshot(
        data,
        m_camera.get(),
        m_gameScene,
        [this](double worldTime) {
            if (m_cameraRigSystem) {
                m_cameraRigSystem->SetWorldTimeSeconds(worldTime);
            }
        });

#if GM_DEBUG_TOOLS
    if (m_debugMenu) {
        m_debugMenu->EndSceneReload();
    }
#endif

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
                        terrain->SetTextureTiling(data.terrainTextureTiling);
                        terrain->SetBaseTextureGuidFromSave(data.terrainBaseTextureGuid);

                        const auto& layers = data.terrainPaintLayers;
                        terrain->SetPaintLayerCount(std::max(1, static_cast<int>(layers.size())));
                        for (std::size_t i = 0; i < layers.size() && i < static_cast<std::size_t>(gm::debug::EditableTerrainComponent::kMaxPaintLayers); ++i) {
                            const auto& layer = layers[i];
                            terrain->SetPaintLayerData(static_cast<int>(i), layer.guid, layer.enabled, layer.weights);
                        }
                        terrain->SetActivePaintLayerIndex(data.terrainActivePaintLayer);
                    }
                }
            }
        }
    }
    
    // Apply FOV if present
    if (data.cameraFov > 0.0f && m_cameraRigSystem) {
        m_cameraRigSystem->SetFovDegrees(data.cameraFov);
    }
    
    ApplyResourcesToScene();
    if (m_cameraRigSystem) {
        m_cameraRigSystem->SetSceneContext(m_gameScene);
    }
#if GM_DEBUG_TOOLS
    if (m_terrainEditingSystem) {
        m_terrainEditingSystem->SetSceneContext(m_gameScene);
    }
#endif
    if (m_questSystem) {
        m_questSystem->SetSceneContext(m_gameScene);
    }
    if (m_toolingFacade) {
        m_toolingFacade->AddNotification(applied ? "QuickLoad applied" : "QuickLoad partially applied");
    }

    if (m_gameScene) {
        m_gameScene->BumpReloadVersion();
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
        if (m_toolingFacade) m_toolingFacade->AddNotification("Resources reloaded");
    } else {
        gm::core::Logger::Warning("[Game] Resource reload encountered errors");
        if (m_toolingFacade) m_toolingFacade->AddNotification("Resource reload failed");
    }
}

bool Game::SetupPrefabs() {
    m_prefabLibrary = std::make_shared<gm::scene::PrefabLibrary>();
    if (m_toolingFacade) {
        m_prefabLibrary->SetMessageCallback([this](const std::string& message, bool isError) {
            if (!m_toolingFacade) {
                return;
            }
            const std::string formatted = isError
                ? fmt::format("Prefab error: {}", message)
                : fmt::format("Prefab warning: {}", message);
            m_toolingFacade->AddNotification(formatted);
        });
    }
    std::filesystem::path prefabRoot = m_assetsDir / "prefabs";
    if (!m_prefabLibrary->LoadDirectory(prefabRoot)) {
        gm::core::Logger::Info("[Game] No prefabs loaded from {}", prefabRoot.string());
    }
    return true;
}

