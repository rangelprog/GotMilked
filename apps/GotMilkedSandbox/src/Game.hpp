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
    namespace core { class InputManager; class InputAction; }
    namespace scene { struct Transform; }
    namespace utils { class CoordinateDisplay; class ImGuiManager; }
}

// Include for GameObject definition (needed for shared_ptr instantiation)
#include "gm/scene/GameObject.hpp"

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
    std::vector<std::shared_ptr<gm::GameObject>> m_cowObjects; // Multiple cows for demonstration

    // resources
    std::unique_ptr<gm::Shader> m_shader;
    std::unique_ptr<gm::Texture> m_cowTex;
    std::unique_ptr<gm::Mesh> m_cowMesh;

    // camera / state
    std::unique_ptr<gm::Camera> m_camera;
    bool m_mouseCaptured = false;
    bool m_firstCapture = true;
    bool m_wireframe = false;
    
    // UI / Debug
    std::unique_ptr<gm::utils::ImGuiManager> m_imguiManager;
    std::unique_ptr<gm::utils::CoordinateDisplay> m_coordDisplay;

    // Helper methods
    void SetupScene();
};
