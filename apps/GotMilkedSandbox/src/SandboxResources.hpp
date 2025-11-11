#pragma once

#include <memory>
#include <string>

#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Texture.hpp"
#include "gm/rendering/Mesh.hpp"

struct SandboxResources {
    std::unique_ptr<gm::Shader> shader;
    std::unique_ptr<gm::Texture> texture;
    std::unique_ptr<gm::Mesh> mesh;

    std::string shaderVertPath;
    std::string shaderFragPath;
    std::string texturePath;
    std::string meshPath;

    bool Load(const std::string& assetsDir);
    void Release();
};

