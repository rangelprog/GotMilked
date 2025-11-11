#pragma once

#include <memory>
#include <string_view>

namespace gm {

class Scene;

/**
 * @brief Base interface for scene-wide systems executed each frame.
 *
 * Systems can participate in the scene lifecycle and optionally run on a
 * background worker when `RunsAsync` returns true.
 */
class SceneSystem {
public:
    virtual ~SceneSystem() = default;

    /**
     * @brief Unique system name used for diagnostics and lookup.
     */
    virtual std::string_view GetName() const = 0;

    /**
     * @brief Called when the system is registered with a scene.
     */
    virtual void OnRegister(Scene& scene) { (void)scene; }

    /**
     * @brief Called when the system is unregistered or the owning scene is
     * shutting down.
     */
    virtual void OnUnregister(Scene& scene) { (void)scene; }

    /**
     * @brief Invoked after the scene has initialized all existing game
     * objects.
     */
    virtual void OnSceneInit(Scene& scene) { (void)scene; }

    /**
     * @brief Invoked before the scene cleans up game objects.
     */
    virtual void OnSceneShutdown(Scene& scene) { (void)scene; }

    /**
     * @brief Called once per frame during the update phase.
     */
    virtual void Update(Scene& scene, float deltaTime) = 0;

    /**
     * @brief Mark the system as asynchronous.
     *
     * When true, `Scene` may execute the system on a worker thread. Systems
     * opting into async execution must handle their own thread-safety.
     */
    virtual bool RunsAsync() const { return false; }
};

using SceneSystemPtr = std::shared_ptr<SceneSystem>;

} // namespace gm


