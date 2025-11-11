#include "GameResources.hpp"

#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Texture.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/utils/ObjLoader.hpp"
#include "gm/utils/ResourceRegistry.hpp"

#include <cstdio>
#include <exception>

bool GameResources::Load(const std::string& assetsDir) {
    shaderGuid = "game_shader";
    textureGuid = "game_texture";
    meshGuid = "game_mesh";

    shaderVertPath = assetsDir + "/shaders/simple.vert.glsl";
    shaderFragPath = assetsDir + "/shaders/simple.frag.glsl";
    texturePath = assetsDir + "/textures/cow.png";  // Using cow texture as demo asset
    meshPath = assetsDir + "/models/cow.obj";       // Using cow mesh as demo asset

    shader = std::make_unique<gm::Shader>();
    if (!shader->loadFromFiles(shaderVertPath, shaderFragPath)) {
        std::printf("[GameResources] Failed to load shader: %s / %s\n",
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

bool GameResources::ReloadShader() {
    if (shaderVertPath.empty() || shaderFragPath.empty()) {
        std::printf("[GameResources] Cannot reload shader: paths not set\n");
        return false;
    }

    auto newShader = std::make_unique<gm::Shader>();
    if (!newShader->loadFromFiles(shaderVertPath, shaderFragPath)) {
        std::printf("[GameResources] Failed to reload shader: %s / %s\n",
                    shaderVertPath.c_str(), shaderFragPath.c_str());
        return false;
    }

    newShader->Use();
    newShader->SetInt("uTex", 0);
    shader = std::move(newShader);

    gm::ResourceRegistry::Instance().RegisterShader(shaderGuid, shaderVertPath, shaderFragPath);
    return true;
}

bool GameResources::ReloadTexture() {
    if (texturePath.empty()) {
        std::printf("[GameResources] Cannot reload texture: path not set\n");
        return false;
    }

    try {
        auto newTexture = std::make_unique<gm::Texture>(gm::Texture::loadOrDie(texturePath, true));
        texture = std::move(newTexture);
        gm::ResourceRegistry::Instance().RegisterTexture(textureGuid, texturePath);
        return true;
    } catch (const std::exception& ex) {
        std::printf("[GameResources] Failed to reload texture %s: %s\n",
                    texturePath.c_str(), ex.what());
    } catch (...) {
        std::printf("[GameResources] Failed to reload texture %s: unknown error\n",
                    texturePath.c_str());
    }
    return false;
}

bool GameResources::ReloadMesh() {
    if (meshPath.empty()) {
        std::printf("[GameResources] Cannot reload mesh: path not set\n");
        return false;
    }

    try {
        auto newMesh = std::make_unique<gm::Mesh>(gm::ObjLoader::LoadObjPNUV(meshPath));
        mesh = std::move(newMesh);
        gm::ResourceRegistry::Instance().RegisterMesh(meshGuid, meshPath);
        return true;
    } catch (const std::exception& ex) {
        std::printf("[GameResources] Failed to reload mesh %s: %s\n",
                    meshPath.c_str(), ex.what());
    } catch (...) {
        std::printf("[GameResources] Failed to reload mesh %s: unknown error\n",
                    meshPath.c_str());
    }
    return false;
}

bool GameResources::ReloadAll() {
    bool ok = true;
    ok = ReloadShader() && ok;
    ok = ReloadTexture() && ok;
    ok = ReloadMesh() && ok;
    return ok;
}

void GameResources::Release() {
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

