#include "apps/GotMilked/src/GameResources.hpp"
#include "gm/utils/Config.hpp"
#include "gm/utils/ResourceManager.hpp"
#include "gm/core/Logger.hpp"
#include "TestAssetHelpers.hpp"

#include <catch2/catch_test_macros.hpp>
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <filesystem>
#include <fstream>
#include <system_error>
#include <stdexcept>
#include <vector>
#include <cstdint>

namespace {

// Helper function to create a minimal valid 1x1 PNG file for testing
std::filesystem::path CreateMinimalPngFile(const std::filesystem::path& dir, const std::string& filename) {
    std::filesystem::path pngPath = dir / filename;
    // Minimal valid 1x1 RGBA PNG
    std::vector<uint8_t> minimalPng = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, // PNG signature
        0x00, 0x00, 0x00, 0x0D, // IHDR chunk length
        0x49, 0x48, 0x44, 0x52, // IHDR
        0x00, 0x00, 0x00, 0x01, // width = 1
        0x00, 0x00, 0x00, 0x01, // height = 1
        0x08, 0x06, 0x00, 0x00, 0x00, // bit depth, color type, compression, filter, interlace
        0x1F, 0x15, 0xC4, 0x89, // CRC
        0x00, 0x00, 0x00, 0x0A, // IDAT chunk length
        0x49, 0x44, 0x41, 0x54, // IDAT
        0x78, 0x9C, 0x63, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00, 0x01, // minimal zlib data
        0x0D, 0x0A, 0x2D, 0xB4, // CRC
        0x00, 0x00, 0x00, 0x00, // IEND chunk length
        0x49, 0x45, 0x4E, 0x44, // IEND
        0xAE, 0x42, 0x60, 0x82  // CRC
    };
    std::ofstream out(pngPath, std::ios::binary);
    out.write(reinterpret_cast<const char*>(minimalPng.data()), minimalPng.size());
    out.close();
    return pngPath;
}

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

TEST_CASE("GameResources loads assets via config", "[game_resources]") {
    GlfwContext glContext;
    TestAssetBundle bundle = CreateMeshSpinnerTestAssets();
    TempDir assets(bundle.root);
    ScopedResourceManagerReset resourceReset;

    // Create a minimal valid PNG file for texture loading
    std::filesystem::path textureFile = CreateMinimalPngFile(bundle.root, "ground.png");
    
    GameResources resources;
    gm::utils::ResourcePathConfig config;
    config.shaderVert = std::filesystem::relative(bundle.vertPath, bundle.root).string();
    config.shaderFrag = std::filesystem::relative(bundle.fragPath, bundle.root).string();
    config.textureGround = std::filesystem::relative(textureFile, bundle.root).string();
    config.meshPlaceholder = std::filesystem::relative(bundle.meshPath, bundle.root).string();

    bool loaded = resources.Load(bundle.root, config);
    REQUIRE(loaded);

    // Verify resources are loaded
    REQUIRE(resources.GetShader() != nullptr);
    REQUIRE(resources.GetTexture() != nullptr);
    REQUIRE(resources.GetMesh() != nullptr);
    REQUIRE(resources.GetTerrainMaterial() != nullptr);

    // Verify GUIDs are set
    REQUIRE_FALSE(resources.GetShaderGuid().empty());
    REQUIRE_FALSE(resources.GetTextureGuid().empty());
    REQUIRE_FALSE(resources.GetMeshGuid().empty());

    // Verify paths are set
    REQUIRE_FALSE(resources.GetShaderVertPath().empty());
    REQUIRE_FALSE(resources.GetShaderFragPath().empty());
    REQUIRE_FALSE(resources.GetTexturePath().empty());
    REQUIRE_FALSE(resources.GetMeshPath().empty());

    // Verify terrain material has texture
    REQUIRE(resources.GetTerrainMaterial()->GetDiffuseTexture() != nullptr);

    resources.Release();
}

TEST_CASE("GameResources legacy load handles missing assets gracefully", "[game_resources]") {
    GlfwContext glContext;
    TestAssetBundle bundle = CreateMeshSpinnerTestAssets();
    TempDir assets(bundle.root);
    ScopedResourceManagerReset resourceReset;

    GameResources resources;
    
    // Create a minimal assets directory structure
    std::filesystem::path assetsDir = bundle.root / "assets";
    std::filesystem::create_directories(assetsDir);
    
    // Copy shader files to expected locations
    std::filesystem::path shaderDir = assetsDir / "shaders";
    std::filesystem::create_directories(shaderDir);
    std::filesystem::copy_file(bundle.vertPath, shaderDir / "simple.vert.glsl");
    std::filesystem::copy_file(bundle.fragPath, shaderDir / "simple.frag.glsl");
    
    // Create texture directory with minimal PNG so legacy load succeeds
    std::filesystem::path texturesDir = assetsDir / "textures";
    std::filesystem::create_directories(texturesDir);
    auto groundTexturePath = CreateMinimalPngFile(texturesDir, "ground.png");
    REQUIRE(std::filesystem::exists(groundTexturePath));
    {
        std::ifstream textureCheck(groundTexturePath, std::ios::binary);
        REQUIRE(textureCheck.good());
    }
    // Provide placeholder mesh at legacy location
    std::filesystem::path modelsDir = assetsDir / "models";
    std::filesystem::create_directories(modelsDir);
    std::filesystem::path placeholderMeshPath = modelsDir / "placeholder.obj";
    std::filesystem::copy_file(bundle.meshPath, placeholderMeshPath);
    REQUIRE(std::filesystem::exists(placeholderMeshPath));

    bool loaded = resources.Load(assetsDir.string());
    REQUIRE(loaded);
    REQUIRE(resources.GetShader() != nullptr);
    REQUIRE(resources.GetTexture() != nullptr);
    REQUIRE(resources.GetTerrainMaterial() != nullptr);
    REQUIRE(resources.GetMesh() != nullptr);

    resources.Release();
}

TEST_CASE("GameResources can reload shaders", "[game_resources][reload]") {
    GlfwContext glContext;
    TestAssetBundle bundle = CreateMeshSpinnerTestAssets();
    TempDir assets(bundle.root);
    ScopedResourceManagerReset resourceReset;

    GameResources resources;
    gm::utils::ResourcePathConfig config;
    config.shaderVert = std::filesystem::relative(bundle.vertPath, bundle.root).string();
    config.shaderFrag = std::filesystem::relative(bundle.fragPath, bundle.root).string();
    std::filesystem::path textureFile = CreateMinimalPngFile(bundle.root, "ground.png");
    config.textureGround = std::filesystem::relative(textureFile, bundle.root).string();
    config.meshPlaceholder = std::filesystem::relative(bundle.meshPath, bundle.root).string();

    bool loaded = resources.Load(bundle.root, config);
    REQUIRE(loaded);

    gm::Shader* originalShader = resources.GetShader();
    REQUIRE(originalShader != nullptr);

    // Reload shader
    bool reloaded = resources.ReloadShader();
    REQUIRE(reloaded);

    gm::Shader* reloadedShader = resources.GetShader();
    REQUIRE(reloadedShader != nullptr);
    // Shader should be a new instance after reload
    REQUIRE(reloadedShader != originalShader);

    resources.Release();
}

TEST_CASE("GameResources can reload textures", "[game_resources][reload]") {
    GlfwContext glContext;
    TestAssetBundle bundle = CreateMeshSpinnerTestAssets();
    TempDir assets(bundle.root);
    ScopedResourceManagerReset resourceReset;

    GameResources resources;
    gm::utils::ResourcePathConfig config;
    config.shaderVert = std::filesystem::relative(bundle.vertPath, bundle.root).string();
    config.shaderFrag = std::filesystem::relative(bundle.fragPath, bundle.root).string();
    std::filesystem::path textureFile = CreateMinimalPngFile(bundle.root, "ground.png");
    config.textureGround = std::filesystem::relative(textureFile, bundle.root).string();
    config.meshPlaceholder = std::filesystem::relative(bundle.meshPath, bundle.root).string();

    bool loaded = resources.Load(bundle.root, config);
    REQUIRE(loaded);

    gm::Texture* originalTexture = resources.GetTexture();
    REQUIRE(originalTexture != nullptr);

    // Reload texture
    bool reloaded = resources.ReloadTexture();
    REQUIRE(reloaded);

    gm::Texture* reloadedTexture = resources.GetTexture();
    REQUIRE(reloadedTexture != nullptr);
    // Texture should be a new instance after reload
    REQUIRE(reloadedTexture != originalTexture);

    // Verify terrain material texture is updated
    REQUIRE(resources.GetTerrainMaterial()->GetDiffuseTexture() == reloadedTexture);

    resources.Release();
}

TEST_CASE("GameResources can reload meshes", "[game_resources][reload]") {
    GlfwContext glContext;
    TestAssetBundle bundle = CreateMeshSpinnerTestAssets();
    TempDir assets(bundle.root);
    ScopedResourceManagerReset resourceReset;

    GameResources resources;
    gm::utils::ResourcePathConfig config;
    config.shaderVert = std::filesystem::relative(bundle.vertPath, bundle.root).string();
    config.shaderFrag = std::filesystem::relative(bundle.fragPath, bundle.root).string();
    std::filesystem::path textureFile = CreateMinimalPngFile(bundle.root, "ground.png");
    config.textureGround = std::filesystem::relative(textureFile, bundle.root).string();
    config.meshPlaceholder = std::filesystem::relative(bundle.meshPath, bundle.root).string();

    bool loaded = resources.Load(bundle.root, config);
    REQUIRE(loaded);

    gm::Mesh* originalMesh = resources.GetMesh();
    REQUIRE(originalMesh != nullptr);

    // Reload mesh
    bool reloaded = resources.ReloadMesh();
    REQUIRE(reloaded);

    gm::Mesh* reloadedMesh = resources.GetMesh();
    REQUIRE(reloadedMesh != nullptr);
    // Mesh should be a new instance after reload
    REQUIRE(reloadedMesh != originalMesh);

    resources.Release();
}

TEST_CASE("GameResources reloads all resources", "[game_resources][reload]") {
    GlfwContext glContext;
    TestAssetBundle bundle = CreateMeshSpinnerTestAssets();
    TempDir assets(bundle.root);
    ScopedResourceManagerReset resourceReset;

    GameResources resources;
    gm::utils::ResourcePathConfig config;
    config.shaderVert = std::filesystem::relative(bundle.vertPath, bundle.root).string();
    config.shaderFrag = std::filesystem::relative(bundle.fragPath, bundle.root).string();
    std::filesystem::path textureFile = CreateMinimalPngFile(bundle.root, "ground.png");
    config.textureGround = std::filesystem::relative(textureFile, bundle.root).string();
    config.meshPlaceholder = std::filesystem::relative(bundle.meshPath, bundle.root).string();

    bool loaded = resources.Load(bundle.root, config);
    REQUIRE(loaded);

    // Reload all resources
    bool reloaded = resources.ReloadAll();
    REQUIRE(reloaded);

    // Verify all resources are still present
    REQUIRE(resources.GetShader() != nullptr);
    REQUIRE(resources.GetTexture() != nullptr);
    REQUIRE(resources.GetMesh() != nullptr);
    REQUIRE(resources.GetTerrainMaterial() != nullptr);

    resources.Release();
}

TEST_CASE("GameResources releases loaded resources", "[game_resources]") {
    GlfwContext glContext;
    TestAssetBundle bundle = CreateMeshSpinnerTestAssets();
    TempDir assets(bundle.root);
    ScopedResourceManagerReset resourceReset;

    GameResources resources;
    gm::utils::ResourcePathConfig config;
    config.shaderVert = std::filesystem::relative(bundle.vertPath, bundle.root).string();
    config.shaderFrag = std::filesystem::relative(bundle.fragPath, bundle.root).string();
    std::filesystem::path textureFile = CreateMinimalPngFile(bundle.root, "ground.png");
    config.textureGround = std::filesystem::relative(textureFile, bundle.root).string();
    config.meshPlaceholder = std::filesystem::relative(bundle.meshPath, bundle.root).string();

    bool loaded = resources.Load(bundle.root, config);
    REQUIRE(loaded);

    // Verify resources are loaded
    REQUIRE(resources.GetShader() != nullptr);
    REQUIRE(resources.GetTexture() != nullptr);
    REQUIRE(resources.GetMesh() != nullptr);
    REQUIRE(resources.GetTerrainMaterial() != nullptr);

    // Release resources
    resources.Release();

    // Verify resources are released
    REQUIRE(resources.GetShader() == nullptr);
    REQUIRE(resources.GetTexture() == nullptr);
    REQUIRE(resources.GetMesh() == nullptr);
    REQUIRE(resources.GetTerrainMaterial() == nullptr);

    // Verify GUIDs and paths are cleared
    REQUIRE(resources.GetShaderGuid().empty());
    REQUIRE(resources.GetTextureGuid().empty());
    REQUIRE(resources.GetMeshGuid().empty());
    REQUIRE(resources.GetShaderVertPath().empty());
    REQUIRE(resources.GetShaderFragPath().empty());
    REQUIRE(resources.GetTexturePath().empty());
    REQUIRE(resources.GetMeshPath().empty());
}

TEST_CASE("GameResources reload methods fail safely when not loaded", "[game_resources][reload]") {
    GlfwContext glContext;
    ScopedResourceManagerReset resourceReset;

    GameResources resources;

    // Try to reload without loading first - should fail gracefully
    bool shaderReloaded = resources.ReloadShader();
    REQUIRE_FALSE(shaderReloaded);

    bool textureReloaded = resources.ReloadTexture();
    REQUIRE_FALSE(textureReloaded);

    bool meshReloaded = resources.ReloadMesh();
    REQUIRE_FALSE(meshReloaded);

    bool allReloaded = resources.ReloadAll();
    REQUIRE_FALSE(allReloaded);
}

TEST_CASE("GameResources load fails with invalid shader paths", "[game_resources][error]") {
    GlfwContext glContext;
    TestAssetBundle bundle = CreateMeshSpinnerTestAssets();
    TempDir assets(bundle.root);
    ScopedResourceManagerReset resourceReset;

    GameResources resources;
    gm::utils::ResourcePathConfig config;
    // Use invalid shader paths
    config.shaderVert = "nonexistent.vert.glsl";
    config.shaderFrag = "nonexistent.frag.glsl";
    std::filesystem::path textureFile = CreateMinimalPngFile(bundle.root, "ground.png");
    config.textureGround = std::filesystem::relative(textureFile, bundle.root).string();
    config.meshPlaceholder = std::filesystem::relative(bundle.meshPath, bundle.root).string();

    bool loaded = resources.Load(bundle.root, config);
    REQUIRE_FALSE(loaded); // Should fail to load

    // Resources should not be loaded
    REQUIRE(resources.GetShader() == nullptr);
}

TEST_CASE("GameResources loads without optional mesh", "[game_resources]") {
    GlfwContext glContext;
    TestAssetBundle bundle = CreateMeshSpinnerTestAssets();
    TempDir assets(bundle.root);
    ScopedResourceManagerReset resourceReset;

    GameResources resources;
    gm::utils::ResourcePathConfig config;
    config.shaderVert = std::filesystem::relative(bundle.vertPath, bundle.root).string();
    config.shaderFrag = std::filesystem::relative(bundle.fragPath, bundle.root).string();
    std::filesystem::path textureFile = CreateMinimalPngFile(bundle.root, "ground.png");
    config.textureGround = std::filesystem::relative(textureFile, bundle.root).string();
    // Use nonexistent mesh path - should still load successfully
    config.meshPlaceholder = "nonexistent.obj";

    bool loaded = resources.Load(bundle.root, config);
    // Should still load successfully even without mesh
    REQUIRE(loaded);

    // Shader and texture should be loaded
    REQUIRE(resources.GetShader() != nullptr);
    REQUIRE(resources.GetTexture() != nullptr);
    REQUIRE(resources.GetTerrainMaterial() != nullptr);

    // Mesh should be null
    REQUIRE(resources.GetMesh() == nullptr);
    REQUIRE(resources.GetMeshPath().empty());

    resources.Release();
}
} // namespace
