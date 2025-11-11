#pragma once

#include <string>
#include <memory>
#include <vector>
struct GLFWwindow;

namespace gm {
    class Shader;
    class Texture;
    class Mesh;
    class Camera;
    class Scene;
    class GameObject;
    namespace core { class InputManager; }
    namespace scene { struct Transform; }
}

#include "gm/scene/GameObject.hpp"
#include "SandboxResources.hpp"

class Game {
public:
    Game(const std::string& assetsDir);
    ~Game();

    bool Init(GLFWwindow* window);
    void Update(float dt);
    void Render();
    void Shutdown();

private:
    std::string m_assetsDir;
    GLFWwindow* m_window = nullptr;

    // Scene management
    std::shared_ptr<gm::Scene> m_gameScene;
    std::vector<std::shared_ptr<gm::GameObject>> m_spinnerObjects; // Mesh spinner objects for demonstration

    SandboxResources m_resources;

    // camera / state
    std::unique_ptr<gm::Camera> m_camera;
    bool m_mouseCaptured = false;
    bool m_firstCapture = true;
    bool m_wireframe = false;
    
    // Helper methods
    void SetupScene();
};
