#include "DebugToolingController.hpp"

#include "Game.hpp"
#include "ToolingFacade.hpp"
#include "GameResources.hpp"
#include "gm/utils/ImGuiManager.hpp"
#include "gm/rendering/Camera.hpp"
#include "gm/tooling/Overlay.hpp"
#include "gm/utils/HotReloader.hpp"
#include "gm/physics/PhysicsWorld.hpp"
#include "gm/core/Logger.hpp"
#include "GameConstants.hpp"

#if GM_DEBUG_TOOLS
#include "DebugMenu.hpp"
#include "DebugHudController.hpp"
#include "EditableTerrainComponent.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/tooling/DebugConsole.hpp"
#include "gm/debug/GridRenderer.hpp"
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#ifdef _WIN32
#ifdef APIENTRY
#undef APIENTRY
#endif
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

DebugToolingController::DebugToolingController(Game& game)
    : m_game(game) {}

bool DebugToolingController::Initialize() {
    m_game.m_imgui = std::make_unique<gm::utils::ImGuiManager>();
    if (m_game.m_imgui && !m_game.m_imgui->Init(m_game.m_window)) {
        gm::core::Logger::Warning("[Game] Failed to initialize ImGui; tooling overlay disabled");
        m_game.m_imgui.reset();
        return false;
    }

    m_game.m_tooling = std::make_unique<gm::tooling::Overlay>();
    if (m_game.m_tooling) {
        gm::tooling::Overlay::Callbacks callbacks{
            [this]() { m_game.PerformQuickSave(); },
            [this]() { m_game.PerformQuickLoad(); },
            [this]() { m_game.ForceResourceReload(); }
        };
        m_game.m_tooling->SetCallbacks(std::move(callbacks));
        m_game.m_tooling->SetSaveManager(m_game.m_saveManager.get());
        m_game.m_tooling->SetHotReloader(&m_game.m_hotReloader);
        m_game.m_tooling->SetCamera(m_game.m_camera.get());
        m_game.m_tooling->SetScene(m_game.m_gameScene);
        m_game.m_tooling->SetPhysicsWorld(&gm::physics::PhysicsWorld::Instance());
        m_game.m_tooling->SetWorldInfoProvider([this]() -> std::optional<gm::tooling::Overlay::WorldInfo> {
            if (!m_game.m_cameraRigSystem || !m_game.m_camera) return std::nullopt;
            gm::tooling::Overlay::WorldInfo info;
            info.sceneName = m_game.m_cameraRigSystem->GetActiveSceneName();
            info.worldTimeSeconds = m_game.m_cameraRigSystem->GetWorldTimeSeconds();
            info.cameraPosition = m_game.m_camera->Position();
            info.cameraDirection = m_game.m_camera->Front();
            return info;
        });
        m_game.m_tooling->AddNotification("Tooling overlay ready");
    }

#if GM_DEBUG_TOOLS
    if (m_game.m_debugHud && m_game.m_tooling) {
        m_game.m_debugHud->SetOverlay(m_game.m_tooling.get());
        m_game.m_debugHud->SetOverlayVisible(m_game.m_overlayVisible);
    }

    m_game.m_debugMenu = std::make_unique<gm::debug::DebugMenu>();
    if (m_game.m_debugMenu) {
        ConfigureDebugMenu();
    }
    m_game.m_debugConsole = std::make_unique<gm::debug::DebugConsole>();
    if (m_game.m_debugMenu) {
        m_game.m_debugMenu->SetDebugConsole(m_game.m_debugConsole.get());
    }
    if (m_game.m_debugHud) {
        m_game.m_debugHud->SetDebugMenu(m_game.m_debugMenu.get());
        m_game.m_debugHud->SetDebugConsole(m_game.m_debugConsole.get());
        m_game.m_debugHud->SetConsoleVisible(false);
        if (m_game.m_debugMenu) {
            m_game.m_debugMenu->SetOverlayToggleCallbacks(
                [this]() -> bool {
                    return m_game.m_debugHud && m_game.m_debugHud->GetOverlayVisible();
                },
                [this](bool visible) {
                    if (m_game.m_debugHud) {
                        m_game.m_debugHud->SetOverlayVisible(visible);
                    }
                });
        }
        m_game.m_debugHud->SetHudVisible(false);
    }

    m_game.m_gridRenderer = std::make_unique<gm::debug::GridRenderer>();
    if (m_game.m_gridRenderer && !m_game.m_gridRenderer->Initialize()) {
        gm::core::Logger::Warning("[Game] Failed to initialize debug grid; disabling grid overlay");
        m_game.m_gridRenderer.reset();
    }
#endif

    if (m_game.m_toolingFacade) {
        m_game.m_toolingFacade->UpdateSceneReference();
        m_game.m_toolingFacade->RefreshHud();
    }

    return true;
}

#if GM_DEBUG_TOOLS
void DebugToolingController::ConfigureDebugMenu() {
    gm::debug::DebugMenu::Callbacks callbacks{
        [this]() { m_game.PerformQuickSave(); },
        [this]() { m_game.PerformQuickLoad(); },
        [this]() { m_game.ForceResourceReload(); },
        [this]() {
            gm::core::Logger::Info("[Game] onSceneLoaded callback called");

            if (m_game.m_gameScene) {
                auto allObjects = m_game.m_gameScene->GetAllGameObjects();
                gm::core::Logger::Info("[Game] Scene has {} GameObjects after load", allObjects.size());
                for (const auto& obj : allObjects) {
                    if (obj) {
                        gm::core::Logger::Info("[Game]   - GameObject: '{}' (active={})",
                                               obj->GetName(), obj->IsActive());
                        auto components = obj->GetComponents();
                        gm::core::Logger::Info("[Game]     Components: {}", components.size());
                        for (const auto& comp : components) {
                            if (comp) {
                                gm::core::Logger::Info("[Game]       - {} (active={})",
                                                       comp->GetName(), comp->IsActive());
                            }
                        }
                    }
                }
            }

            m_game.ApplyResourcesToScene();
            if (m_game.m_debugMenu && m_game.m_gameScene) {
                auto terrainObject = m_game.m_gameScene->FindGameObjectByName("Terrain");
                if (terrainObject) {
                    if (auto terrain = terrainObject->GetComponent<gm::debug::EditableTerrainComponent>()) {
                        m_game.m_debugMenu->SetTerrainComponent(terrain.get());
                        if (m_game.m_debugHud) {
                            m_game.m_debugHud->RegisterTerrain(terrain.get());
                        }
                    }
                }
            }
        },
        [this]() -> glm::vec3 {
            return m_game.m_camera ? m_game.m_camera->Position() : glm::vec3(0.0f);
        },
        [this]() -> glm::vec3 {
            return m_game.m_camera ? m_game.m_camera->Front() : glm::vec3(0.0f, 0.0f, -1.0f);
        },
        [this]() -> float {
            return m_game.m_cameraRigSystem ? m_game.m_cameraRigSystem->GetFovDegrees()
                                            : gotmilked::GameConstants::Camera::DefaultFovDegrees;
        },
        [this](const glm::vec3& position, const glm::vec3& forward, float fov) {
            if (m_game.m_camera) {
                m_game.m_camera->SetPosition(position);
                m_game.m_camera->SetForward(forward);
            }
            if (m_game.m_cameraRigSystem && fov > 0.0f) {
                m_game.m_cameraRigSystem->SetFovDegrees(fov);
            }
        },
        [this]() -> double {
            return m_game.m_cameraRigSystem ? m_game.m_cameraRigSystem->GetWorldTimeSeconds() : 0.0;
        },
        [this]() -> glm::mat4 {
            return m_game.m_camera ? m_game.m_camera->View() : glm::mat4(1.0f);
        },
        [this]() -> glm::mat4 {
            if (!m_game.m_window || !m_game.m_cameraRigSystem) {
                return glm::mat4(1.0f);
            }
            int fbw, fbh;
            glfwGetFramebufferSize(m_game.m_window, &fbw, &fbh);
            if (fbw <= 0 || fbh <= 0) {
                return glm::mat4(1.0f);
            }
            float aspect = static_cast<float>(fbw) / static_cast<float>(fbh);
            float fov = m_game.m_cameraRigSystem ? m_game.m_cameraRigSystem->GetFovDegrees()
                                                 : gotmilked::GameConstants::Camera::DefaultFovDegrees;
            return glm::perspective(glm::radians(fov),
                                    aspect,
                                    gotmilked::GameConstants::Camera::NearPlane,
                                    gotmilked::GameConstants::Camera::FarPlane);
        },
        [this](int& width, int& height) {
            if (m_game.m_window) {
                glfwGetFramebufferSize(m_game.m_window, &width, &height);
            } else {
                width = 0;
                height = 0;
            }
        }
    };
    m_game.m_debugMenu->SetCallbacks(std::move(callbacks));
    m_game.m_debugMenu->SetSaveManager(m_game.m_saveManager.get());
    m_game.m_debugMenu->SetScene(m_game.m_gameScene);
    m_game.m_debugMenu->SetPrefabLibrary(m_game.m_prefabLibrary.get());
    m_game.m_debugMenu->SetGameResources(&m_game.m_resources);

#ifdef _WIN32
    if (m_game.m_window) {
        HWND hwnd = glfwGetWin32Window(m_game.m_window);
        m_game.m_debugMenu->SetWindowHandle(hwnd);
    }
#endif

    if (m_game.m_gameScene) {
        auto terrainObject = m_game.m_gameScene->FindGameObjectByName("Terrain");
        if (terrainObject) {
            if (auto terrain = terrainObject->GetComponent<gm::debug::EditableTerrainComponent>()) {
                m_game.m_debugMenu->SetTerrainComponent(terrain.get());
                if (m_game.m_debugHud) {
                    m_game.m_debugHud->RegisterTerrain(terrain.get());
                }
            }
        }
    }

    m_game.m_debugMenu->LoadRecentFilesFromDisk();
}
#endif

