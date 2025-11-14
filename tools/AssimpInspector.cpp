#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <fmt/core.h>

#include <filesystem>
#include <iostream>
#include <cstdlib>

namespace {

void PrintUsage() {
    fmt::print("Usage: AssimpInspector <path-to-model.glb>\n");
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        PrintUsage();
        return EXIT_FAILURE;
    }

    const std::filesystem::path inputPath = argv[1];
    if (!std::filesystem::exists(inputPath)) {
        fmt::print(stderr, "Error: file '{}' does not exist.\n", inputPath.string());
        return EXIT_FAILURE;
    }

    Assimp::Importer importer;
    const unsigned int flags = aiProcess_Triangulate |
                               aiProcess_JoinIdenticalVertices |
                               aiProcess_LimitBoneWeights |
                               aiProcess_SortByPType |
                               aiProcess_CalcTangentSpace;

    const aiScene* scene = importer.ReadFile(inputPath.string(), flags);
    if (!scene) {
        fmt::print(stderr, "Assimp failed to load '{}': {}\n", inputPath.string(), importer.GetErrorString());
        return EXIT_FAILURE;
    }

    fmt::print("Loaded '{}'\n", inputPath.string());
    fmt::print("  Meshes:        {}\n", scene->mNumMeshes);
    fmt::print("  Materials:     {}\n", scene->mNumMaterials);
    fmt::print("  Animations:    {}\n", scene->mNumAnimations);

    if (scene->mNumMeshes > 0) {
        fmt::print("\nMeshes:\n");
        for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
            const aiMesh* mesh = scene->mMeshes[i];
            fmt::print("  [{}] '{}': vertices={}, faces={}, bones={}\n",
                       i,
                       mesh->mName.C_Str(),
                       mesh->mNumVertices,
                       mesh->mNumFaces,
                       mesh->mNumBones);
        }
    }

    if (scene->mNumAnimations > 0) {
        fmt::print("\nAnimations:\n");
        for (unsigned int i = 0; i < scene->mNumAnimations; ++i) {
            const aiAnimation* anim = scene->mAnimations[i];
            const double duration = anim->mDuration / (anim->mTicksPerSecond > 0.0 ? anim->mTicksPerSecond : 1.0);
            fmt::print("  [{}] '{}': duration={:.3f}s, channels={}\n",
                       i,
                       anim->mName.C_Str(),
                       duration,
                       anim->mNumChannels);
        }
    }

    return EXIT_SUCCESS;
}

