#include "TestAssetHelpers.hpp"

#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Texture.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/utils/ObjLoader.hpp"

#include <fstream>
#include <random>
#include <vector>
#include <system_error>

namespace {

std::filesystem::path MakeTempDirectory() {
    auto base = std::filesystem::temp_directory_path();
    std::random_device rd;
    std::uniform_int_distribution<int> dist(0, 0xFFFFFF);
    std::filesystem::path dir;
    do {
        dir = base / ("GotMilkedSandbox_" + std::to_string(dist(rd)));
    } while (std::filesystem::exists(dir));
    std::filesystem::create_directories(dir);
    return dir;
}

void WriteFile(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream out(path);
    out << contents;
    out.close();
}

} // namespace

TestAssetBundle CreateMeshSpinnerTestAssets() {
    TestAssetBundle bundle;
    bundle.root = MakeTempDirectory();

    const std::string vertSrc =
        "#version 330 core\n"
        "layout(location = 0) in vec3 aPos;\n"
        "uniform mat4 uModel;\n"
        "uniform mat4 uView;\n"
        "uniform mat4 uProj;\n"
        "void main() {\n"
        "    gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);\n"
        "}\n";

    const std::string fragSrc =
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "void main() {\n"
        "    FragColor = vec4(0.2, 0.6, 0.9, 1.0);\n"
        "}\n";

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

void PopulateSandboxResourcesFromTestAssets(const TestAssetBundle& bundle, SandboxResources& resources) {
    resources.shader = std::make_unique<gm::Shader>();
    if (!resources.shader->loadFromFiles(bundle.vertPath, bundle.fragPath)) {
        throw std::runtime_error("Failed to load shader from test assets");
    }
    resources.shader->Use();
    resources.shader->SetInt("uTex", 0);

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
    resources.texture = std::move(texture);

    resources.mesh = std::make_unique<gm::Mesh>(gm::ObjLoader::LoadObjPNUV(bundle.meshPath));

    resources.shaderVertPath = bundle.vertPath;
    resources.shaderFragPath = bundle.fragPath;
    resources.texturePath = bundle.textureTag;
    resources.meshPath = bundle.meshPath;
}

