#include "gm/utils/ResourceManager.hpp"
#include "gm/core/Logger.hpp"

#include "TestAssetHelpers.hpp"

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cassert>
#include <filesystem>
#include <iostream>
#include <system_error>
#include <stdexcept>

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
        m_window = glfwCreateWindow(64, 64, "ResourceManagerTests", nullptr, nullptr);
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

void ResourceManagerCacheAndReload() {
    GlfwContext glContext;
    TestAssetBundle bundle = CreateMeshSpinnerTestAssets();
    TempDir assets(bundle.root);

    gm::ResourceManager::Init();

    auto shader = gm::ResourceManager::LoadShader("test_shader",
                                                  bundle.vertPath,
                                                  bundle.fragPath);
    assert(shader);
    assert(gm::ResourceManager::HasShader("test_shader"));

    auto shaderAgain = gm::ResourceManager::LoadShader("test_shader",
                                                       bundle.vertPath,
                                                       bundle.fragPath);
    assert(shaderAgain);
    assert(shaderAgain == shader);

    auto shaderReload = gm::ResourceManager::ReloadShader("test_shader",
                                                          bundle.vertPath,
                                                          bundle.fragPath);
    assert(shaderReload);
    assert(gm::ResourceManager::GetShader("test_shader") == shaderReload);

    auto mesh = gm::ResourceManager::LoadMesh("test_mesh", bundle.meshPath);
    assert(mesh);
    assert(gm::ResourceManager::HasMesh("test_mesh"));

    auto meshAgain = gm::ResourceManager::LoadMesh("test_mesh", bundle.meshPath);
    assert(meshAgain == mesh);

    auto meshReload = gm::ResourceManager::ReloadMesh("test_mesh", bundle.meshPath);
    assert(meshReload);
    assert(gm::ResourceManager::GetMesh("test_mesh") == meshReload);

    gm::ResourceManager::Cleanup();
}

} // namespace

void RunResourceManagerCacheAndReloadTest() {
    ResourceManagerCacheAndReload();
    std::cout << "ResourceManager cache/reload test passed.\n";
}

