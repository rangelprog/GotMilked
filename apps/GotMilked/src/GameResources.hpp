#pragma once

#include <memory>
#include <string>

#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Texture.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/rendering/Material.hpp"

struct GameResources {
    std::unique_ptr<gm::Shader> shader;
    std::unique_ptr<gm::Texture> texture;
    std::unique_ptr<gm::Mesh> mesh;

    std::unique_ptr<gm::Mesh> planeMesh;
    std::unique_ptr<gm::Mesh> cubeMesh;
    std::shared_ptr<gm::Material> planeMaterial;
    std::shared_ptr<gm::Material> cubeMaterial;

    std::string shaderGuid;
    std::string shaderVertPath;
    std::string shaderFragPath;
    std::string textureGuid;
    std::string texturePath;
    std::string meshGuid;
    std::string meshPath;

    bool Load(const std::string& assetsDir);
    bool ReloadShader();
    bool ReloadTexture();
    bool ReloadMesh();
    bool ReloadAll();
    void Release();
};

