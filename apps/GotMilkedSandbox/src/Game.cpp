#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "Game.hpp"
#include "MeshSpinnerComponent.hpp"
#include "SandboxSceneHelpers.hpp"
#include "SceneSerializerExtensions.hpp"

#include <cstdio>
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
#include "gm/core/input/InputManager.hpp"

namespace {
static float &FovRef() { static float fov = 60.0f; return fov; }
} // namespace

Game::Game(const std::string& assetsDir)
    : m_assetsDir(assetsDir) {}

Game::~Game() = default;

bool Game::Init(GLFWwindow* window) {
    m_window = window;

    if (!m_resources.Load(m_assetsDir)) {
        return false;
    }

    gm::SceneSerializerExtensions::RegisterSerializers();

    m_camera = std::make_unique<gm::Camera>();

    using namespace gm::core;
    auto& inputManager = InputManager::Instance();
    inputManager.Init(window);

    SetupScene();

    return true;
}

void Game::SetupScene() {
    auto& sceneManager = gm::SceneManager::Instance();

    m_gameScene = sceneManager.LoadScene("GameScene");
    if (!m_gameScene) {
        std::printf("[Game] Error: Failed to create game scene\n");
        return;
    }

    sceneManager.InitActiveScene();
    std::printf("[Game] Game scene initialized successfully\n");

    sandbox::RehydrateMeshSpinnerComponents(*m_gameScene, m_resources, m_camera.get());
    sandbox::CollectMeshSpinnerObjects(*m_gameScene, m_spinnerObjects);

    if (m_spinnerObjects.empty()) {
        sandbox::PopulateSandboxScene(*m_gameScene, *m_camera, m_resources, m_spinnerObjects);
    } else {
        std::printf("[Game] Loaded %zu mesh spinner objects from scene\n", m_spinnerObjects.size());
    }
}

void Game::Update(float dt) {
    if (!m_window) return;

    // Update the scene (updates all GameObjects)
    auto& sceneManager = gm::SceneManager::Instance();
    sceneManager.UpdateActiveScene(dt);

    // Get input system (Update() already called in main loop before glfwPollEvents)
    auto& inputManager = gm::core::InputManager::Instance();
    auto inputSys = inputManager.GetInputSystem();

    // Close window on ESC
    if (inputSys->IsKeyJustPressed(GLFW_KEY_ESCAPE))
        glfwSetWindowShouldClose(m_window, 1);

    // RMB capture toggle - check if button is held while captured, or just pressed
    if (!m_mouseCaptured && inputSys->IsMouseButtonJustPressed(gm::core::MouseButton::Right)) {
        // Start capture on RMB press
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        m_mouseCaptured = true;
        m_firstCapture = true;
    } else if (m_mouseCaptured && inputSys->IsMouseButtonJustReleased(gm::core::MouseButton::Right)) {
        // End capture on RMB release
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        m_mouseCaptured = false;
    }

    // Mouse look with delta
    if (m_mouseCaptured) {
        if (m_firstCapture) {
            m_firstCapture = false;
        }
        double dx = inputSys->GetMouseDeltaX();
        double dy = inputSys->GetMouseDeltaY();
        m_camera->ProcessMouseMovement((float)dx, (float)dy);
    }

    // Movement with speed boost for Shift
    const float baseSpeed = 3.0f;
    const float speed = baseSpeed * (inputSys->IsKeyPressed(GLFW_KEY_LEFT_SHIFT) ? 4.0f : 1.0f) * dt;
    
    if (inputSys->IsKeyPressed(GLFW_KEY_W)) m_camera->MoveForward(speed);
    if (inputSys->IsKeyPressed(GLFW_KEY_S)) m_camera->MoveBackward(speed);
    if (inputSys->IsKeyPressed(GLFW_KEY_A)) m_camera->MoveLeft(speed);
    if (inputSys->IsKeyPressed(GLFW_KEY_D)) m_camera->MoveRight(speed);
    if (inputSys->IsKeyPressed(GLFW_KEY_SPACE)) m_camera->MoveUp(speed);
    if (inputSys->IsKeyPressed(GLFW_KEY_LEFT_CONTROL)) m_camera->MoveDown(speed);

    // Wireframe toggle on F key
    if (inputSys->IsKeyJustPressed(GLFW_KEY_F)) {
        m_wireframe = !m_wireframe;
        glPolygonMode(GL_FRONT_AND_BACK, m_wireframe ? GL_LINE : GL_FILL);
    }

    // Handle scroll for FOV
    double scrollY = inputSys->GetMouseScrollY();
    if (scrollY != 0.0) {
        FovRef() -= (float)scrollY * 2.0f;
        if (FovRef() < 30.0f) FovRef() = 30.0f;
        if (FovRef() > 100.0f) FovRef() = 100.0f;
    }
}

void Game::Render() {
    if (!m_window || !m_resources.shader) return;

    int fbw, fbh;
    glfwGetFramebufferSize(m_window, &fbw, &fbh);
    glViewport(0, 0, fbw, fbh);
    glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = (float)fbw / (float)fbh;
    float fov = FovRef();
    glm::mat4 proj = glm::perspective(glm::radians(fov), aspect, 0.1f, 200.0f);
    glm::mat4 view = m_camera->View();

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
    
}

void Game::Shutdown() {
    // Clear spinner object references (they'll be destroyed with the scene)
    m_spinnerObjects.clear();
    
    // Clean up scene and scene manager
    m_gameScene.reset();
    gm::SceneManager::Instance().Shutdown();

    gm::SceneSerializerExtensions::UnregisterSerializers();
    m_resources.Release();
    
    m_camera.reset();
    
    // InputManager is a singleton, no need to manually reset
    printf("[Game] Shutdown complete\n");
}
