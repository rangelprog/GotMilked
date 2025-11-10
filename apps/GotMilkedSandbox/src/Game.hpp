#pragma once

#include <string>
#include <memory>
struct GLFWwindow;

namespace gm {
class Shader;
class Texture;
class Mesh;
class Camera;
namespace core { class InputManager; class InputAction; }
namespace scene { struct Transform; }
}

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

    // resources
    std::unique_ptr<gm::Shader> m_shader;
    std::unique_ptr<gm::Texture> m_cowTex;
    std::unique_ptr<gm::Mesh> m_cowMesh;

    // camera / state
    std::unique_ptr<gm::Camera> m_camera;
    bool m_mouseCaptured = false;
    bool m_firstCapture = true;
    bool m_wireframe = false;
};
