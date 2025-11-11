#include "SandboxResources.hpp"

#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Texture.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/utils/ObjLoader.hpp"
#include "gm/utils/ResourceRegistry.hpp"

#include <cstdio>

bool SandboxResources::Load(const std::string& assetsDir) {
    shaderGuid = "sandbox_shader";
    textureGuid = "sandbox_texture";
    meshGuid = "sandbox_mesh";

    shaderVertPath = assetsDir + "/shaders/simple.vert.glsl";
    shaderFragPath = assetsDir + "/shaders/simple.frag.glsl";
    texturePath = assetsDir + "/textures/cow.png";  // Using cow texture as demo asset
    meshPath = assetsDir + "/models/cow.obj";       // Using cow mesh as demo asset

    shader = std::make_unique<gm::Shader>();
    if (!shader->loadFromFiles(shaderVertPath, shaderFragPath)) {
        std::printf("[SandboxResources] Failed to load shader: %s / %s\n",
                    shaderVertPath.c_str(), shaderFragPath.c_str());
        shader.reset();
        return false;
    }

    shader->Use();
    shader->SetInt("uTex", 0);

    texture = std::make_unique<gm::Texture>(gm::Texture::loadOrDie(texturePath, true));
    mesh = std::make_unique<gm::Mesh>(gm::ObjLoader::LoadObjPNUV(meshPath));

    auto& registry = gm::ResourceRegistry::Instance();
    registry.RegisterShader(shaderGuid, shaderVertPath, shaderFragPath);
    registry.RegisterTexture(textureGuid, texturePath);
    registry.RegisterMesh(meshGuid, meshPath);

    return true;
}

void SandboxResources::Release() {
    auto& registry = gm::ResourceRegistry::Instance();
    registry.UnregisterShader(shaderGuid);
    registry.UnregisterTexture(textureGuid);
    registry.UnregisterMesh(meshGuid);

    shader.reset();
    texture.reset();
    mesh.reset();
    shaderGuid.clear();
    shaderVertPath.clear();
    shaderFragPath.clear();
    textureGuid.clear();
    texturePath.clear();
    meshGuid.clear();
    meshPath.clear();
}

