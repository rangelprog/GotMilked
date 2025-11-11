#include "TestAssetHelpers.hpp"

#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Texture.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/rendering/Material.hpp"
#include "gm/prototypes/Primitives.hpp"
#include "gm/utils/ObjLoader.hpp"
#include "gm/utils/ResourceRegistry.hpp"

#include <fstream>
#include <iterator>
#include <random>
#include <vector>
#include <system_error>
#include <glm/vec3.hpp>

namespace {

std::filesystem::path MakeTempDirectory() {
    auto base = std::filesystem::temp_directory_path();
    std::random_device rd;
    std::uniform_int_distribution<int> dist(0, 0xFFFFFF);
    std::filesystem::path dir;
    do {
        dir = base / ("GotMilked_" + std::to_string(dist(rd)));
    } while (std::filesystem::exists(dir));
    std::filesystem::create_directories(dir);
    return dir;
}

std::string LoadTextFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to read file: " + path.string());
    }
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteFile(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to write file: " + path.string());
    }
    out << contents;
}

} // namespace

TestAssetBundle CreateMeshSpinnerTestAssets() {
    TestAssetBundle bundle;
    bundle.root = MakeTempDirectory();

    const std::filesystem::path shaderDir = std::filesystem::path(GM_GAME_SHADER_DIR);
    const std::string vertSrc = LoadTextFile(shaderDir / "simple.vert.glsl");
    const std::string fragSrc = LoadTextFile(shaderDir / "simple.frag.glsl");

    const std::string objContent =
        "v -0.5 -0.5 0.0\n"
        "v  0.5 -0.5 0.0\n"
        "v  0.0  0.5 0.0\n"
        "vn 0.0 0.0 1.0\n"
        "vt 0.0 0.0\n"
        "vt 1.0 0.0\n"
        "vt 0.5 0.5\n"
        "f 1/1/1 2/2/1 3/3/1\n";

    bundle.vertPath = (bundle.root / "test.vert.glsl").string();
    bundle.fragPath = (bundle.root / "test.frag.glsl").string();
    bundle.meshPath = (bundle.root / "triangle.obj").string();
    bundle.textureTag = (bundle.root / "procedural_texture").string();

    WriteFile(bundle.vertPath, vertSrc);
    WriteFile(bundle.fragPath, fragSrc);
    WriteFile(bundle.meshPath, objContent);

    return bundle;
}

void PopulateGameResourcesFromTestAssets(const TestAssetBundle& bundle, GameResources& resources) {
    // Friend function - can access private members
    resources.m_shader = std::make_unique<gm::Shader>();
    if (!resources.m_shader->loadFromFiles(bundle.vertPath, bundle.fragPath)) {
        throw std::runtime_error("Failed to load shader from test assets");
    }
    resources.m_shader->Use();
    resources.m_shader->SetInt("uTex", 0);

    auto texture = std::make_unique<gm::Texture>();
    std::vector<std::uint8_t> pixels = {
        255, 0, 0, 255,
        0, 255, 0, 255,
        0, 0, 255, 255,
        255, 255, 0, 255
    };
    if (!texture->createRGBA8(2, 2, pixels, false)) {
        throw std::runtime_error("Failed to create procedural texture");
    }
    resources.m_texture = std::move(texture);

    resources.m_mesh = std::make_unique<gm::Mesh>(gm::ObjLoader::LoadObjPNUV(bundle.meshPath));
    resources.m_planeMesh = std::make_unique<gm::Mesh>(gm::prototypes::CreatePlane(5.0f, 5.0f, 2.0f));
    resources.m_cubeMesh = std::make_unique<gm::Mesh>(gm::prototypes::CreateCube(1.0f));

    resources.m_planeMaterial = std::make_shared<gm::Material>(gm::Material::CreatePhong(glm::vec3(0.4f, 0.7f, 0.4f),
                                                                                       glm::vec3(0.2f), 16.0f));
    resources.m_planeMaterial->SetDiffuseTexture(resources.m_texture.get());
    resources.m_cubeMaterial = std::make_shared<gm::Material>(gm::Material::CreatePhong(glm::vec3(0.75f, 0.25f, 0.25f),
                                                                                      glm::vec3(0.4f), 32.0f));

    resources.m_shaderGuid = "test_shader_" + bundle.root.filename().string();
    resources.m_textureGuid = "test_texture_" + bundle.root.filename().string();
    resources.m_meshGuid = "test_mesh_" + bundle.root.filename().string();
    resources.m_shaderVertPath = bundle.vertPath;
    resources.m_shaderFragPath = bundle.fragPath;
    resources.m_texturePath = bundle.textureTag;
    resources.m_meshPath = bundle.meshPath;

    auto& registry = gm::ResourceRegistry::Instance();
    registry.RegisterShader(resources.m_shaderGuid, resources.m_shaderVertPath, resources.m_shaderFragPath);
    registry.RegisterTexture(resources.m_textureGuid, resources.m_texturePath);
    registry.RegisterMesh(resources.m_meshGuid, resources.m_meshPath);
}

