#include "CameraRigSystem.hpp"

#include "CameraRigComponent.hpp"
#include "gm/rendering/Camera.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/core/Logger.hpp"

#include <algorithm>

namespace {
constexpr std::string_view kSystemName = "CameraRigSystem";
}

namespace gm::gameplay {

CameraRigSystem::CameraRigSystem() = default;

std::string_view CameraRigSystem::GetName() const {
    return kSystemName;
}

void CameraRigSystem::OnRegister(gm::Scene& scene) {
    m_lastSceneVersion = scene.CurrentReloadVersion();
    RefreshRigInstances(scene);
}

void CameraRigSystem::OnSceneInit(gm::Scene& scene) {
    m_lastSceneVersion = scene.CurrentReloadVersion();
    RefreshRigInstances(scene);
}

void CameraRigSystem::OnSceneShutdown(gm::Scene& /*scene*/) {
    m_rigs.clear();
}

void CameraRigSystem::SetActiveCamera(gm::Camera* camera) {
    if (m_camera == camera) {
        return;
    }
    m_camera = camera;
    InvalidateControllers();
}

void CameraRigSystem::SetWindow(GLFWwindow* window) {
    m_window = window;
    for (auto& rig : m_rigs) {
        if (rig.controller) {
            rig.controller->SetWindow(window);
        }
    }
}

void CameraRigSystem::SetSceneContext(const std::shared_ptr<gm::Scene>& scene) {
    m_sceneWeak = scene;
    if (scene) {
        m_lastSceneVersion = scene->CurrentReloadVersion();
        RefreshRigInstances(*scene);
    }
}

void CameraRigSystem::SetInputSuppressed(bool suppressed) {
    m_inputSuppressed = suppressed;
    for (auto& rig : m_rigs) {
        if (rig.controller) {
            rig.controller->SetInputSuppressed(suppressed);
        }
    }
}

void CameraRigSystem::SetWireframeCallback(FlyCameraController::WireframeCallback callback) {
    m_wireframeCallback = std::move(callback);
    for (auto& rig : m_rigs) {
        if (rig.controller) {
            rig.controller->SetWireframeCallback(m_wireframeCallback);
        }
    }
}

float CameraRigSystem::GetFovDegrees() const {
    for (const auto& rig : m_rigs) {
        if (rig.controller) {
            return rig.controller->GetFovDegrees();
        }
    }
    return m_cachedFov;
}

void CameraRigSystem::SetFovDegrees(float fov) {
    bool applied = false;
    for (auto& rig : m_rigs) {
        if (rig.controller) {
            rig.controller->SetFovDegrees(fov);
            applied = true;
        }
    }
    if (!applied) {
        m_cachedFov = fov;
    }
}

double CameraRigSystem::GetWorldTimeSeconds() const {
    for (const auto& rig : m_rigs) {
        if (rig.controller) {
            return rig.controller->GetWorldTimeSeconds();
        }
    }
    return m_cachedWorldTime;
}

void CameraRigSystem::SetWorldTimeSeconds(double seconds) {
    bool applied = false;
    for (auto& rig : m_rigs) {
        if (rig.controller) {
            rig.controller->SetWorldTimeSeconds(seconds);
            applied = true;
        }
    }
    if (!applied) {
        m_cachedWorldTime = seconds;
    }
}

bool CameraRigSystem::IsMouseCaptured() const {
    for (const auto& rig : m_rigs) {
        if (rig.controller && rig.controller->IsMouseCaptured()) {
            return true;
        }
    }
    return false;
}

std::string CameraRigSystem::GetActiveSceneName() const {
    for (const auto& rig : m_rigs) {
        if (rig.controller) {
            return rig.controller->GetActiveSceneName();
        }
    }
    if (auto scene = m_sceneWeak.lock()) {
        return scene->GetName();
    }
    return {};
}

void CameraRigSystem::Update(gm::Scene& scene, float deltaTime) {
    if (scene.CurrentReloadVersion() != m_lastSceneVersion) {
        m_lastSceneVersion = scene.CurrentReloadVersion();
        RefreshRigInstances(scene);
    } else {
        PruneExpiredRigs();
    }

    EnsureControllers();

    for (auto& rig : m_rigs) {
        auto component = rig.component.lock();
        auto* controller = rig.controller.get();
        if (!component || !controller) {
            continue;
        }

        ApplySharedState(*controller);
        controller->Update(deltaTime);
    }

    if (!m_rigs.empty() && m_rigs.front().controller) {
        m_cachedFov = m_rigs.front().controller->GetFovDegrees();
        m_cachedWorldTime = m_rigs.front().controller->GetWorldTimeSeconds();
    }
}

void CameraRigSystem::RefreshRigInstances(gm::Scene& scene) {
    m_rigs.clear();
    auto& objects = scene.GetAllGameObjects();
    for (auto& object : objects) {
        if (!object) {
            continue;
        }
        auto rigComponent = object->GetComponent<CameraRigComponent>();
        if (!rigComponent) {
            continue;
        }

        RigInstance instance;
        instance.component = rigComponent;
        m_rigs.push_back(std::move(instance));
    }

    InvalidateControllers();
}

void CameraRigSystem::PruneExpiredRigs() {
    auto it = std::remove_if(m_rigs.begin(), m_rigs.end(),
        [](RigInstance& instance) {
            if (instance.component.expired()) {
                return true;
            }
            return false;
        });
    if (it != m_rigs.end()) {
        m_rigs.erase(it, m_rigs.end());
    }
}

void CameraRigSystem::EnsureControllers() {
    if (!m_camera) {
        return;
    }

    auto sceneShared = m_sceneWeak.lock();

    for (auto& rig : m_rigs) {
        if (rig.controller) {
            continue;
        }

        auto component = rig.component.lock();
        if (!component) {
            continue;
        }

        rig.controller = std::make_unique<FlyCameraController>(*m_camera, m_window, component->GetConfig());
        if (sceneShared) {
            rig.controller->SetScene(sceneShared);
        }
        rig.controller->SetInputSuppressed(m_inputSuppressed);
        if (m_wireframeCallback) {
            rig.controller->SetWireframeCallback(m_wireframeCallback);
        }
        if (m_cachedFov != component->GetInitialFov()) {
            rig.controller->SetFovDegrees(m_cachedFov);
        }
        if (m_cachedWorldTime > 0.0) {
            rig.controller->SetWorldTimeSeconds(m_cachedWorldTime);
        }
    }
}

void CameraRigSystem::ApplySharedState(FlyCameraController& controller) {
    controller.SetInputSuppressed(m_inputSuppressed);
    controller.SetWindow(m_window);
    if (auto sceneShared = m_sceneWeak.lock()) {
        controller.SetScene(sceneShared);
    }
    if (m_wireframeCallback) {
        controller.SetWireframeCallback(m_wireframeCallback);
    }
}

void CameraRigSystem::InvalidateControllers() {
    for (auto& rig : m_rigs) {
        rig.controller.reset();
    }
}

} // namespace gm::gameplay


