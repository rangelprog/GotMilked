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

    // Scroll callback (FOV)
    glfwSetScrollCallback(window, [](GLFWwindow*, double, double yoff) {
        FovRef() -= (float)yoff * 2.0f;
        if (FovRef() < 30.0f) FovRef() = 30.0f;
        if (FovRef() > 100.0f) FovRef() = 100.0f;
    });

    return true;
}

void Game::Update(float dt) {
    if (!m_window) return;

    // Close
    if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(m_window, 1);

    // RMB capture toggle
    if (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        if (!m_mouseCaptured) {
            glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            m_mouseCaptured = true;
            m_firstCapture = true;
        }
    } else if (m_mouseCaptured) {
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        m_mouseCaptured = false;
    }

    // Mouse look
    if (m_mouseCaptured) {
        double mx, my;
        glfwGetCursorPos(m_window, &mx, &my);
        if (m_firstCapture) {
            m_lastMouseX = mx; m_lastMouseY = my; m_firstCapture = false;
        }
        double dx = mx - m_lastMouseX;
        double dy = m_lastMouseY - my;
        m_lastMouseX = mx; m_lastMouseY = my;
        m_camera->ProcessMouseMovement((float)dx, (float)dy);
    }

    // Movement
    const float baseSpeed = 3.0f;
    const float speed = baseSpeed * ((glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ? 4.0f : 1.0f) * dt;
    if (glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS) m_camera->MoveForward(speed);
    if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS) m_camera->MoveBackward(speed);
    if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS) m_camera->MoveLeft(speed);
    if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS) m_camera->MoveRight(speed);
    if (glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS) m_camera->MoveUp(speed);
    if (glfwGetKey(m_window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) m_camera->MoveDown(speed);

    // Toggles
    static bool pf = false;
    bool fNow = glfwGetKey(m_window, GLFW_KEY_F) == GLFW_PRESS;
    if (fNow && !pf) { m_wireframe = !m_wireframe; glPolygonMode(GL_FRONT_AND_BACK, m_wireframe ? GL_LINE : GL_FILL); }
    pf = fNow;
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
}
