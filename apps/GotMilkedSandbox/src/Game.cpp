#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "Game.hpp"
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "gm/rendering/Camera.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/utils/ObjLoader.hpp"
#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Texture.hpp"
#include "gm/scene/Transform.hpp"
#include "gm/core/input/InputManager.hpp"
#include "gm/core/input/InputAction.hpp"

namespace {
static float &FovRef() { static float fov = 60.0f; return fov; }
}

Game::Game(const std::string& assetsDir)
    : m_assetsDir(assetsDir) {}

Game::~Game() { }

bool Game::Init(GLFWwindow* window) {
    m_window = window;

    // Shader
    const std::string shaderDir = m_assetsDir + "/shaders";
    const std::string vertPath = shaderDir + "/simple.vert.glsl";
    const std::string fragPath = shaderDir + "/simple.frag.glsl";
    m_shader = std::make_unique<gm::Shader>();
    if (!m_shader->loadFromFiles(vertPath, fragPath)) {
        std::printf("Failed to load shaders from %s / %s\n", vertPath.c_str(), fragPath.c_str());
        return false;
    }

    // Texture
    const std::string cowTexPath = m_assetsDir + "/textures/cow.png";
    m_cowTex = std::make_unique<gm::Texture>(gm::Texture::loadOrDie(cowTexPath, true));
    m_shader->Use();
    m_shader->SetInt("uTex", 0);

    // Mesh
    const std::string cowObjPath = m_assetsDir + "/models/cow.obj";
    m_cowMesh = std::make_unique<gm::Mesh>(gm::ObjLoader::LoadObjPNUV(cowObjPath));

    // Camera
    m_camera = std::make_unique<gm::Camera>();

    // Initialize InputManager (singleton)
    using namespace gm::core;
    auto& inputManager = InputManager::Instance();
    inputManager.Init(window);

    // Create movement action (WASD + Space/Ctrl)
    auto moveAction = inputManager.CreateAction("Move");
    moveAction->AddBinding({InputType::Keyboard, GLFW_KEY_W, InputTriggerType::WhilePressed});
    moveAction->AddBinding({InputType::Keyboard, GLFW_KEY_S, InputTriggerType::WhilePressed});
    moveAction->AddBinding({InputType::Keyboard, GLFW_KEY_A, InputTriggerType::WhilePressed});
    moveAction->AddBinding({InputType::Keyboard, GLFW_KEY_D, InputTriggerType::WhilePressed});
    moveAction->AddBinding({InputType::Keyboard, GLFW_KEY_SPACE, InputTriggerType::WhilePressed});
    moveAction->AddBinding({InputType::Keyboard, GLFW_KEY_LEFT_CONTROL, InputTriggerType::WhilePressed});

    // Create capture action (RMB for mouse capture)
    auto captureAction = inputManager.CreateAction("Capture");
    captureAction->AddBinding({InputType::MouseButton, static_cast<int>(gm::core::MouseButton::Right), InputTriggerType::WhilePressed});

    // Create wireframe toggle (F key)
    auto wireframeAction = inputManager.CreateAction("Wireframe");
    wireframeAction->AddBinding({InputType::Keyboard, GLFW_KEY_F, InputTriggerType::OnPress});

    return true;
}

void Game::Update(float dt) {
    if (!m_window) return;

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
    if (!m_window || !m_shader) return;

    int fbw, fbh;
    glfwGetFramebufferSize(m_window, &fbw, &fbh);
    glViewport(0, 0, fbw, fbh);
    glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = (float)fbw / (float)fbh;
    float fov = FovRef();
    glm::mat4 proj = glm::perspective(glm::radians(fov), aspect, 0.1f, 200.0f);
    glm::mat4 view = m_camera->View();

    gm::Transform T;
    T.scale = glm::vec3(1.0f);
    T.rotation.y = (float)glfwGetTime() * 20.0f;
    glm::mat4 model = T.getMatrix();
    glm::mat3 normalMat = glm::mat3(glm::transpose(glm::inverse(model)));

    m_shader->Use();
    m_shader->SetMat4("uModel", model);
    m_shader->SetMat4("uView", view);
    m_shader->SetMat4("uProj", proj);

    if (GLint loc = m_shader->uniformLoc("uNormalMat"); loc >= 0)
        glUniformMatrix3fv(loc, 1, GL_FALSE, glm::value_ptr(normalMat));

    if (GLint loc = m_shader->uniformLoc("uViewPos"); loc >= 0)
        glUniform3fv(loc, 1, glm::value_ptr(m_camera->Position()));

    if (GLint loc = m_shader->uniformLoc("uLightDir"); loc >= 0)
        glUniform3fv(loc, 1, glm::value_ptr(glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f))));
    if (GLint loc = m_shader->uniformLoc("uLightColor"); loc >= 0)
        glUniform3fv(loc, 1, glm::value_ptr(glm::vec3(1.0f)));

    if (GLint loc = m_shader->uniformLoc("uUseTex"); loc >= 0)
        glUniform1i(loc, 1);
    m_cowTex->bind(0);

    m_cowMesh->Draw();
}

void Game::Shutdown() {
    m_cowMesh.reset();
    m_cowTex.reset();
    m_shader.reset();
    m_camera.reset();
    // InputManager is a singleton, no need to manually reset
}
