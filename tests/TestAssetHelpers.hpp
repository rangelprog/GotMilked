#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "apps/GotMilked/src/GameResources.hpp"

namespace gm {
class Shader;
class Texture;
class Mesh;
}

struct TestAssetBundle {
    std::filesystem::path root;
    std::string vertPath;
    std::string fragPath;
    std::string meshPath;
    std::string textureTag;
};

TestAssetBundle CreateMeshSpinnerTestAssets();

void PopulateGameResourcesFromTestAssets(const TestAssetBundle& bundle, GameResources& resources);

