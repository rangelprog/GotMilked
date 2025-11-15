#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <deque>

#include <glm/vec3.hpp>

namespace gm {
class Camera;
class Scene;
}

namespace gm::save {
struct SaveMetadata;
class SaveManager;
}

namespace gm::utils {
class HotReloader;
}

namespace gm::physics {
class PhysicsWorld;
}

namespace gm::tooling {

class Overlay {
public:
    struct Callbacks {
        std::function<void()> quickSave;
        std::function<void()> quickLoad;
        std::function<void()> reloadResources;
        std::function<void(const std::string&)> applyProfilingPreset;
    };

    struct WorldInfo {
        std::string sceneName;
        double worldTimeSeconds = 0.0;
        glm::vec3 cameraPosition{0.0f};
        glm::vec3 cameraDirection{0.0f, 0.0f, -1.0f};
    };

    using WorldInfoProvider = std::function<std::optional<WorldInfo>()>;
    struct NarrativeEntry {
        enum class Type {
            Quest,
            Dialogue
        };

        Type type = Type::Quest;
        std::string identifier;
        std::string subject;
        glm::vec3 location{0.0f};
        bool repeatable = false;
        bool sceneLoad = false;
        bool autoStart = false;
        std::chrono::system_clock::time_point timestamp{};
    };
    using NarrativeLogProvider = std::function<std::vector<NarrativeEntry>()>;

    struct WeatherForecastEntry {
        std::string profile;
        float startHour = 0.0f;
        float durationHours = 0.0f;
        std::string description;
    };

    struct WeatherInfo {
        float normalizedTime = 0.0f;
        float dayLengthSeconds = 0.0f;
        std::string activeProfile;
        float windSpeed = 0.0f;
        glm::vec3 windDirection{0.0f};
        float surfaceWetness = 0.0f;
        float puddleAmount = 0.0f;
        float surfaceDarkening = 0.0f;
        glm::vec3 surfaceTint{1.0f};
        std::vector<std::string> alerts;
        std::vector<WeatherForecastEntry> forecast;
    };
    using WeatherInfoProvider = std::function<std::optional<WeatherInfo>()>;

    void SetCallbacks(Callbacks callbacks) { m_callbacks = std::move(callbacks); }
    void SetSaveManager(gm::save::SaveManager* manager);
    void SetHotReloader(gm::utils::HotReloader* reloader) { m_hotReloader = reloader; }
    void SetCamera(gm::Camera* camera) { m_camera = camera; }
    void SetScene(const std::shared_ptr<gm::Scene>& scene);
    void SetWorldInfoProvider(WorldInfoProvider provider) { m_worldInfoProvider = std::move(provider); }
    void SetPhysicsWorld(gm::physics::PhysicsWorld* physics) { m_physicsWorld = physics; }
    void SetNarrativeLogProvider(NarrativeLogProvider provider) { m_narrativeProvider = std::move(provider); }
    void SetWeatherInfoProvider(WeatherInfoProvider provider) { m_weatherProvider = std::move(provider); }
    void SetProfilingPresetCallback(std::function<void(const std::string&)> callback) { m_callbacks.applyProfilingPreset = std::move(callback); }

    void AddNotification(const std::string& message);

    void Render(bool& overlayOpen);

private:
    void RenderActionsSection();
    void RenderHotReloadSection();
    void RenderSaveSection();
    void RenderWorldSection();
    void RenderNarrativeSection();
    void RenderWeatherSection();
    void RenderProfilingSection();
    void RenderPhysicsSection();
    void RenderNotifications();
    void RefreshSaveList();
    void PruneNotifications();

    gm::save::SaveManager* m_saveManager = nullptr;
    gm::utils::HotReloader* m_hotReloader = nullptr;
    gm::Camera* m_camera = nullptr;
    gm::physics::PhysicsWorld* m_physicsWorld = nullptr;
    std::weak_ptr<gm::Scene> m_scene;

    Callbacks m_callbacks;
    WorldInfoProvider m_worldInfoProvider;
    NarrativeLogProvider m_narrativeProvider;
    WeatherInfoProvider m_weatherProvider;

    std::deque<std::pair<std::chrono::system_clock::time_point, std::string>> m_notifications;
    std::vector<gm::save::SaveMetadata> m_cachedSaves;
    std::chrono::system_clock::time_point m_lastSaveRefresh{};
};

} // namespace gm::tooling

