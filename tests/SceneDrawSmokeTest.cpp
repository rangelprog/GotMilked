#include "gm/scene/Scene.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/rendering/Camera.hpp"

#include "GameSceneHelpers.hpp"
#include "TestAssetHelpers.hpp"

#include <catch2/catch_test_macros.hpp>
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <filesystem>
#include <stdexcept>
#include <system_error>

namespace {

class GlfwContext {
public:
    GlfwContext() {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        m_window = glfwCreateWindow(128, 128, "SceneDrawSmokeTest", nullptr, nullptr);
        if (!m_window) {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }
        glfwMakeContextCurrent(m_window);
        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
            glfwDestroyWindow(m_window);
            glfwTerminate();
            throw std::runtime_error("Failed to load GLAD");
        }
    }

    ~GlfwContext() {
        if (m_window) {
            glfwDestroyWindow(m_window);
            glfwTerminate();
        }
    }

private:
    GLFWwindow* m_window = nullptr;
};

class TempDir {
public:
    explicit TempDir(std::filesystem::path dir) : path(std::move(dir)) {}

    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    std::filesystem::path path;
};

} // namespace

TEST_CASE("Scene draws without errors", "[scene][rendering][smoke]") {
    GlfwContext context;
    TestAssetBundle bundle = CreateMeshSpinnerTestAssets();
    TempDir assets(bundle.root);

    GameResources resources;
    PopulateGameResourcesFromTestAssets(bundle, resources);
    REQUIRE(resources.GetShader());
    REQUIRE(resources.GetMesh());

    gm::Scene scene("DrawScene");
    gm::Camera camera({0.0f, 0.0f, 0.0f});

    gotmilked::PopulateInitialScene(scene, camera, resources);
    scene.Init();

    auto* shader = resources.GetShader();
    REQUIRE(shader);
    shader->Use();

    REQUIRE_NOTHROW(scene.Draw(*shader, camera, 128, 128, 60.0f));

    resources.Release();
}

TEST_CASE("Smoketest scene loads gameplay actors", "[scene][smoketest]") {
    GlfwContext context;
    TestAssetBundle bundle = CreateMeshSpinnerTestAssets();
    TempDir assets(bundle.root);

    GameResources resources;
    PopulateGameResourcesFromTestAssets(bundle, resources);
    REQUIRE(resources.GetShader());
    REQUIRE(resources.GetMesh());

    gm::Scene scene("SmoketestScene");
    gm::Camera camera({0.0f, 1.5f, 4.0f});

    gotmilked::PopulateSmoketestScene(scene, camera, resources);

    auto npcA = scene.FindGameObjectByName("QuestGiver_A");
    auto npcB = scene.FindGameObjectByName("QuestGiver_B");
    REQUIRE(npcA);
    REQUIRE(npcB);

    auto truck = scene.FindGameObjectByName("BarnTruck");
    auto tractor = scene.FindGameObjectByName("FieldTractor");
    REQUIRE(truck);
    REQUIRE(tractor);

    resources.Release();
}

