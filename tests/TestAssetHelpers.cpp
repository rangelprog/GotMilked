#include "TestAssetHelpers.hpp"

#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Texture.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/rendering/Material.hpp"
#include "gm/prototypes/Primitives.hpp"
#include "gm/utils/ObjLoader.hpp"
#include "gm/utils/ResourceManifest.hpp"
#include "gm/utils/ResourceRegistry.hpp"
#include "gm/utils/ResourceManager.hpp"

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
    resources.Release();
    resources.m_lastError.reset();
    resources.m_assetsDir = bundle.root;

    auto& registry = gm::ResourceRegistry::Instance();

    const std::string shaderGuid = "test_shader_" + bundle.root.filename().string();
    gm::ResourceManager::ShaderDescriptor shaderDescriptor{
        shaderGuid,
        bundle.vertPath,
        bundle.fragPath
    };
    auto shaderHandle = gm::ResourceManager::LoadShader(shaderDescriptor);
    auto shader = shaderHandle.Lock();
    if (!shader) {
        throw std::runtime_error("Failed to load shader from test assets");
    }
    shader->Use();
    shader->SetInt("uTex", 0);

    resources.m_shaders[shaderGuid] = shader;
    resources.m_shaderSources[shaderGuid] = GameResources::ShaderSources{bundle.vertPath, bundle.fragPath};
    resources.m_defaultShaderGuid = shaderGuid;
    resources.m_defaultShaderVertPath = bundle.vertPath;
    resources.m_defaultShaderFragPath = bundle.fragPath;
    registry.RegisterShader(shaderGuid, bundle.vertPath, bundle.fragPath);

    const std::string textureGuid = "test_texture_" + bundle.root.filename().string();
    auto texture = std::make_shared<gm::Texture>();
    std::vector<std::uint8_t> pixels = {
        255, 0, 0, 255,
        0, 255, 0, 255,
        0, 0, 255, 255,
        255, 255, 0, 255
    };
    if (!texture->createRGBA8(2, 2, pixels, false)) {
        throw std::runtime_error("Failed to create procedural texture");
    }

    gm::utils::ResourceManifest::TextureEntry textureEntry;
    textureEntry.guid = textureGuid;
    textureEntry.path = bundle.textureTag;

    resources.m_textures[textureGuid] = texture;
    resources.m_textureSources[textureGuid] = textureEntry;
    resources.m_defaultTextureGuid = textureGuid;
    resources.m_defaultTexturePath = textureEntry.path;
    registry.RegisterTexture(textureGuid, textureEntry.path);

    const std::string meshGuid = "test_mesh_" + bundle.root.filename().string();
    gm::ResourceManager::MeshDescriptor meshDescriptor{
        meshGuid,
        bundle.meshPath
    };
    auto meshHandle = gm::ResourceManager::LoadMesh(meshDescriptor);
    auto mesh = meshHandle.Lock();
    if (!mesh) {
        throw std::runtime_error("Failed to load mesh from test assets");
    }

    gm::utils::ResourceManifest::MeshEntry meshEntry;
    meshEntry.guid = meshGuid;
    meshEntry.path = bundle.meshPath;

    resources.m_meshes[meshGuid] = mesh;
    resources.m_meshSources[meshGuid] = meshEntry;
    resources.m_defaultMeshGuid = meshGuid;
    resources.m_defaultMeshPath = bundle.meshPath;
    registry.RegisterMesh(meshGuid, bundle.meshPath);

    const std::string materialGuid = "test_terrain_material_" + bundle.root.filename().string();
    gm::utils::ResourceManifest::MaterialEntry materialEntry;
    materialEntry.guid = materialGuid;
    materialEntry.name = "Test Terrain Material";
    materialEntry.diffuseColor = glm::vec3(0.4f, 0.7f, 0.4f);
    materialEntry.specularColor = glm::vec3(0.2f);
    materialEntry.emissionColor = glm::vec3(0.0f);
    materialEntry.shininess = 16.0f;
    materialEntry.diffuseTextureGuid = textureGuid;

    auto material = std::make_shared<gm::Material>(gm::Material::CreatePhong(materialEntry.diffuseColor,
                                                                             materialEntry.specularColor,
                                                                             materialEntry.shininess));
    material->SetName(materialEntry.name);
    material->SetDiffuseTexture(texture.get());

    resources.m_materials[materialGuid] = material;
    resources.m_materialSources[materialGuid] = materialEntry;
    resources.m_defaultTerrainMaterialGuid = materialGuid;
    registry.RegisterMaterial(materialGuid, gm::ResourceRegistry::MaterialData{
        materialEntry.name,
        materialEntry.diffuseColor,
        materialEntry.specularColor,
        materialEntry.emissionColor,
        materialEntry.shininess,
        materialEntry.diffuseTextureGuid,
        materialEntry.specularTextureGuid,
        materialEntry.normalTextureGuid,
        materialEntry.emissionTextureGuid
    });
}

