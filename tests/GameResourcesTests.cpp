#include "apps/GotMilked/src/GameResources.hpp"
#include "gm/utils/ResourceManager.hpp"
#include "gm/core/Logger.hpp"
#include "gm/core/Error.hpp"
#include "TestAssetHelpers.hpp"

#include <catch2/catch_test_macros.hpp>
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <filesystem>
#include <fstream>
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
        m_window = glfwCreateWindow(64, 64, "GameResourcesTests", nullptr, nullptr);
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

struct ScopedResourceManagerReset {
    ScopedResourceManagerReset() {
        gm::ResourceManager::Cleanup();
        gm::ResourceManager::Init();
    }
    ~ScopedResourceManagerReset() { gm::ResourceManager::Cleanup(); }
};

std::filesystem::path SetupBasicAssets(const TestAssetBundle& bundle) {
    std::filesystem::path assetsDir = bundle.root / "assets";
    std::filesystem::create_directories(assetsDir / "shaders");
    std::filesystem::create_directories(assetsDir / "models");
    std::filesystem::create_directories(assetsDir / "prefabs");

    std::filesystem::copy_file(bundle.vertPath,
                               assetsDir / "shaders" / "simple.vert.glsl",
                               std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file(bundle.fragPath,
                               assetsDir / "shaders" / "simple.frag.glsl",
                               std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file(bundle.meshPath,
                               assetsDir / "models" / "cow.obj",
                               std::filesystem::copy_options::overwrite_existing);

    const char* prefabContent = R"JSON(
{
  "name": "TestPrefab",
  "components": []
}
)JSON";
    std::ofstream prefabFile(assetsDir / "prefabs" / "TestPrefab.json", std::ios::binary);
    prefabFile << prefabContent;
    prefabFile.close();

    return assetsDir;
}

TEST_CASE("GameResources loads catalog assets", "[game_resources]") {
    GlfwContext glContext;
    TestAssetBundle bundle = CreateMeshSpinnerTestAssets();
    TempDir assets(bundle.root);
    ScopedResourceManagerReset resourceReset;

    std::filesystem::path assetsDir = SetupBasicAssets(bundle);

    GameResources resources;
    bool loaded = resources.Load(assetsDir);
    REQUIRE(loaded);
    REQUIRE(resources.GetLastError() == nullptr);

    // Verify resources are loaded from directories
    REQUIRE(resources.GetShader() != nullptr);
    REQUIRE(resources.GetMesh() != nullptr);
    REQUIRE(resources.GetTexture() == nullptr);
    REQUIRE(resources.GetTerrainMaterial() == nullptr);

    // Verify defaults are set for loaded types
    REQUIRE_FALSE(resources.GetShaderGuid().empty());
    REQUIRE_FALSE(resources.GetMeshGuid().empty());
    REQUIRE(resources.GetTextureGuid().empty());

    REQUIRE_FALSE(resources.GetShaderVertPath().empty());
    REQUIRE_FALSE(resources.GetShaderFragPath().empty());
    REQUIRE_FALSE(resources.GetMeshPath().empty());
    REQUIRE(resources.GetTexturePath().empty());

    // Prefabs detected
    REQUIRE_FALSE(resources.GetPrefabMap().empty());

    resources.Release();
    REQUIRE(resources.GetLastError() == nullptr);
}

TEST_CASE("GameResources ignores manifest overrides", "[game_resources]") {
    GlfwContext glContext;
    TestAssetBundle bundle = CreateMeshSpinnerTestAssets();
    TempDir assets(bundle.root);
    ScopedResourceManagerReset resourceReset;

    std::filesystem::path assetsDir = SetupBasicAssets(bundle);
    // Duplicate shader files with custom names referenced by the manifest
    std::filesystem::copy_file(assetsDir / "shaders" / "simple.vert.glsl",
                               assetsDir / "shaders" / "custom.vert.glsl",
                               std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file(assetsDir / "shaders" / "simple.frag.glsl",
                               assetsDir / "shaders" / "custom.frag.glsl",
                               std::filesystem::copy_options::overwrite_existing);

    const char* manifestContents = R"JSON(
{
  "shaders": {
    "custom_shader": {
      "vertex": "shaders/custom.vert.glsl",
      "fragment": "shaders/custom.frag.glsl"
    }
  },
  "defaults": {
    "shader": "custom_shader",
    "mesh": "missing_mesh"
  }
}
)JSON";

    std::ofstream manifestFile(assetsDir / "resources.json", std::ios::binary);
    manifestFile << manifestContents;
    manifestFile.close();

    GameResources resources;
    REQUIRE(resources.Load(assetsDir));
    REQUIRE(resources.GetLastError() == nullptr);

    REQUIRE(resources.GetShaderMap().count("custom_shader") == 0);
    REQUIRE(resources.GetShaderGuid() != "custom_shader");
    REQUIRE(resources.GetMeshGuid() != "missing_mesh");

    resources.Release();
    REQUIRE(resources.GetLastError() == nullptr);
}

TEST_CASE("GameResources reloads individual resources", "[game_resources][reload]") {
    GlfwContext glContext;
    TestAssetBundle bundle = CreateMeshSpinnerTestAssets();
    TempDir assets(bundle.root);
    ScopedResourceManagerReset resourceReset;

    GameResources resources;
    std::filesystem::path assetsDir = SetupBasicAssets(bundle);

    bool loaded = resources.Load(assetsDir);
    REQUIRE(loaded);
    REQUIRE(resources.GetLastError() == nullptr);

    gm::Shader* originalShader = resources.GetShader();
    REQUIRE(originalShader != nullptr);

    // Reload shader
    bool shaderReloaded = resources.ReloadShader();
    REQUIRE(shaderReloaded);
    REQUIRE(resources.GetLastError() == nullptr);

    gm::Shader* reloadedShader = resources.GetShader();
    REQUIRE(reloadedShader != nullptr);
    // Shader should be a new instance after reload
    REQUIRE(reloadedShader != originalShader);

    gm::Mesh* originalMesh = resources.GetMesh();
    REQUIRE(originalMesh != nullptr);

    bool meshReloaded = resources.ReloadMesh();
    REQUIRE(meshReloaded);
    REQUIRE(resources.GetLastError() == nullptr);

    gm::Mesh* reloadedMesh = resources.GetMesh();
    REQUIRE(reloadedMesh != nullptr);
    REQUIRE(reloadedMesh != originalMesh);

    resources.Release();
    REQUIRE(resources.GetLastError() == nullptr);
}

TEST_CASE("GameResources reloads all resources", "[game_resources][reload]") {
    GlfwContext glContext;
    TestAssetBundle bundle = CreateMeshSpinnerTestAssets();
    TempDir assets(bundle.root);
    ScopedResourceManagerReset resourceReset;

    GameResources resources;
    std::filesystem::path assetsDir = SetupBasicAssets(bundle);

    bool loaded = resources.Load(assetsDir);
    REQUIRE(loaded);
    REQUIRE(resources.GetLastError() == nullptr);

    gm::Shader* originalShader = resources.GetShader();
    gm::Mesh* originalMesh = resources.GetMesh();

    // Reload all resources
    bool reloaded = resources.ReloadAll();
    REQUIRE(reloaded);
    REQUIRE(resources.GetLastError() == nullptr);

    // Verify all resources are still present
    REQUIRE(resources.GetShader() != nullptr);
    REQUIRE(resources.GetMesh() != nullptr);

    // Ensure new instances were produced
    REQUIRE(resources.GetShader() != originalShader);
    REQUIRE(resources.GetMesh() != originalMesh);

    resources.Release();
    REQUIRE(resources.GetLastError() == nullptr);
}

TEST_CASE("GameResources releases loaded resources", "[game_resources]") {
    GlfwContext glContext;
    TestAssetBundle bundle = CreateMeshSpinnerTestAssets();
    TempDir assets(bundle.root);
    ScopedResourceManagerReset resourceReset;

    GameResources resources;
    std::filesystem::path assetsDir = SetupBasicAssets(bundle);

    bool loaded = resources.Load(assetsDir);
    REQUIRE(loaded);
    REQUIRE(resources.GetLastError() == nullptr);

    // Verify resources are loaded
    REQUIRE(resources.GetShader() != nullptr);
    REQUIRE(resources.GetMesh() != nullptr);

    // Release resources
    resources.Release();
    REQUIRE(resources.GetLastError() == nullptr);

    // Verify resources are released
    REQUIRE(resources.GetShader() == nullptr);
    REQUIRE(resources.GetMesh() == nullptr);

    // Verify GUIDs and paths are cleared
    REQUIRE(resources.GetShaderGuid().empty());
    REQUIRE(resources.GetMeshGuid().empty());
    REQUIRE(resources.GetShaderVertPath().empty());
    REQUIRE(resources.GetShaderFragPath().empty());
    REQUIRE(resources.GetMeshPath().empty());
}

TEST_CASE("GameResources reload methods fail safely when not loaded", "[game_resources][reload]") {
    GlfwContext glContext;
    ScopedResourceManagerReset resourceReset;

    GameResources resources;

    // Try to reload without loading first - should fail gracefully
    bool shaderReloaded = resources.ReloadShader();
    REQUIRE_FALSE(shaderReloaded);
    REQUIRE(resources.GetLastError() != nullptr);

    bool textureReloaded = resources.ReloadTexture();
    REQUIRE_FALSE(textureReloaded);
    REQUIRE(resources.GetLastError() != nullptr);

    bool meshReloaded = resources.ReloadMesh();
    REQUIRE_FALSE(meshReloaded);
    REQUIRE(resources.GetLastError() != nullptr);

    bool allReloaded = resources.ReloadAll();
    REQUIRE_FALSE(allReloaded);
    REQUIRE(resources.GetLastError() != nullptr);
}

TEST_CASE("GameResources ignores invalid manifest entries", "[game_resources]") {
    GlfwContext glContext;
    TestAssetBundle bundle = CreateMeshSpinnerTestAssets();
    TempDir assets(bundle.root);
    ScopedResourceManagerReset resourceReset;

    GameResources resources;
    std::filesystem::path assetsDir = SetupBasicAssets(bundle);

    std::ofstream manifest(assetsDir / "resources.json", std::ios::binary);
    manifest << R"JSON(
{
  "shaders": {
    "broken_shader": {
      "vertex": "shaders/missing.vert.glsl",
      "fragment": "shaders/missing.frag.glsl"
    }
  }
}
)JSON";
    manifest.close();

    bool loaded = resources.Load(assetsDir);
    REQUIRE(loaded);
    REQUIRE(resources.GetLastError() == nullptr);

    REQUIRE(resources.GetShaderMap().count("broken_shader") == 0);
    resources.Release();
}

TEST_CASE("GameResources loads without optional mesh", "[game_resources]") {
    GlfwContext glContext;
    TestAssetBundle bundle = CreateMeshSpinnerTestAssets();
    TempDir assets(bundle.root);
    ScopedResourceManagerReset resourceReset;

    GameResources resources;
    std::filesystem::path assetsDir = SetupBasicAssets(bundle);
    std::filesystem::remove(assetsDir / "models" / "cow.obj");

    bool loaded = resources.Load(assetsDir);
    // Should still load successfully even without mesh
    REQUIRE(loaded);
    REQUIRE(resources.GetLastError() == nullptr);

    // Shader should be loaded; mesh optional
    REQUIRE(resources.GetShader() != nullptr);
    REQUIRE(resources.GetMesh() == nullptr);
    REQUIRE(resources.GetMeshPath().empty());

    resources.Release();
    REQUIRE(resources.GetLastError() == nullptr);
}
} // namespace
