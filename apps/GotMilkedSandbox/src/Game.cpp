#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "Game.hpp"
#include "CowRendererComponent.hpp"
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "gm/rendering/Camera.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/utils/ObjLoader.hpp"
#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Texture.hpp"
#include "gm/scene/Transform.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/MaterialComponent.hpp"
#include "gm/scene/LightComponent.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/scene/SceneManager.hpp"
#include "gm/rendering/Material.hpp"
#include "gm/core/input/InputManager.hpp"
#include "gm/core/input/InputAction.hpp"
#include "gm/utils/CoordinateDisplay.hpp"
#include "gm/utils/ImGuiManager.hpp"

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

    // Initialize ImGui
    m_imguiManager = std::make_unique<gm::utils::ImGuiManager>();
    if (!m_imguiManager->Init(window)) {
        printf("[Game] Warning: Failed to initialize ImGui\n");
    }

    // Initialize Coordinate Display
    m_coordDisplay = std::make_unique<gm::utils::CoordinateDisplay>();

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

    // Initialize scene system
    SetupScene();

    return true;
}

void Game::SetupScene() {
    auto& sceneManager = gm::SceneManager::Instance();
    
    // Create and load the game scene
    m_gameScene = sceneManager.LoadScene("GameScene");
    
    if (!m_gameScene) {
        printf("[Game] Error: Failed to create game scene\n");
        return;
    }
    
    // Initialize the scene
    sceneManager.InitActiveScene();
    printf("[Game] Game scene initialized successfully\n");

    // Create multiple cows to demonstrate the component system
    const int numCows = 3;
    const float spacing = 3.0f;
    
    for (int i = 0; i < numCows; ++i) {
        std::string cowName = "Cow_" + std::to_string(i + 1);
        
        // Create the cow GameObject
        auto cowObject = m_gameScene->CreateGameObject(cowName);
        cowObject->AddTag("animal");
        cowObject->AddTag("cow");
        
        // Ensure TransformComponent exists and position the cow
        auto transform = cowObject->EnsureTransform();
        float xPos = (i - (numCows - 1) / 2.0f) * spacing;
        transform->SetPosition(xPos, 0.0f, -5.0f);
        transform->SetScale(1.0f); // Uniform scale
        
        // Create and assign material
        auto material = std::make_shared<gm::Material>();
        material->SetName("Cow Material " + std::to_string(i + 1));
        material->SetDiffuseTexture(m_cowTex.get());
        material->SetDiffuseColor(glm::vec3(1.0f, 1.0f, 1.0f));
        material->SetSpecularColor(glm::vec3(0.3f, 0.3f, 0.3f));
        material->SetShininess(32.0f);

        // Add MaterialComponent
        auto materialComp = cowObject->AddComponent<gm::MaterialComponent>();
        materialComp->SetMaterial(material);
        
        // Add the CowRendererComponent
        auto cowRenderer = cowObject->AddComponent<CowRendererComponent>();
        cowRenderer->SetMesh(m_cowMesh.get());
        cowRenderer->SetTexture(m_cowTex.get()); // Keep for backward compatibility
        cowRenderer->SetShader(m_shader.get());
        cowRenderer->SetCamera(m_camera.get());
        
        // Vary rotation speed for each cow
        cowRenderer->SetRotationSpeed(15.0f + i * 5.0f);
        
        cowRenderer->Init();
        
        // Verify component was added correctly
        if (cowObject->HasComponent<CowRendererComponent>()) {
            printf("[Game] %s created at position (%.1f, 0.0, -5.0)\n", 
                   cowName.c_str(), xPos);
        }
        
        m_cowObjects.push_back(cowObject);
    }

    // Create a directional light (sun)
    auto sunLight = m_gameScene->CreateGameObject("Sun");
    auto sunTransform = sunLight->EnsureTransform();
    sunTransform->SetPosition(0.0f, 10.0f, 0.0f);
    auto sunLightComp = sunLight->AddComponent<gm::LightComponent>();
    sunLightComp->SetType(gm::LightComponent::LightType::Directional);
    sunLightComp->SetDirection(glm::vec3(-0.4f, -1.0f, -0.3f));
    sunLightComp->SetColor(glm::vec3(1.0f, 1.0f, 1.0f));
    sunLightComp->SetIntensity(1.5f); // Increased intensity
    printf("[Game] Created directional light (Sun)\n");

    // Demonstrate finding objects by tag
    auto animals = m_gameScene->FindGameObjectsByTag("animal");
    printf("[Game] Found %zu animals in scene\n", animals.size());
    
    printf("[Game] Scene setup complete with %zu cows and 1 light\n", m_cowObjects.size());
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

    // Update projection matrix on all CowRendererComponents
    for (auto& cowObject : m_cowObjects) {
        if (cowObject && cowObject->HasComponent<CowRendererComponent>()) {
            if (auto cowRenderer = cowObject->GetComponent<CowRendererComponent>()) {
                cowRenderer->SetProjectionMatrix(proj);
            }
        }
    }

    // Render the game scene with all its GameObjects
    if (m_gameScene && m_shader && m_camera) {
        m_gameScene->Draw(*m_shader, *m_camera, fbw, fbh, fov);
    }
    
    // Start ImGui frame
    if (m_imguiManager && m_imguiManager->IsInitialized()) {
        m_imguiManager->NewFrame();
        
        // Render coordinate display
        if (m_coordDisplay && m_camera) {
            m_coordDisplay->Render(*m_camera, fov);
        }
        
        // Render ImGui
        m_imguiManager->Render();
    }
}

void Game::Shutdown() {
    // Clear cow object references (they'll be destroyed with the scene)
    m_cowObjects.clear();
    
    // Clean up scene and scene manager
    m_gameScene.reset();
    gm::SceneManager::Instance().Shutdown();

    // Clean up resources
    m_imguiManager.reset();
    m_coordDisplay.reset();
    m_cowMesh.reset();
    m_cowTex.reset();
    m_shader.reset();
    m_camera.reset();
    
    // InputManager is a singleton, no need to manually reset
    printf("[Game] Shutdown complete\n");
}
