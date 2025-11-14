#include "gm/animation/Skeleton.hpp"
#include "gm/animation/AnimationClip.hpp"
#include "gm/animation/SkinnedMeshAsset.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/material.h>

#include <fmt/core.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

using gm::animation::AnimationClip;
using gm::animation::Skeleton;
using gm::animation::SkinnedMeshAsset;

struct ImportOptions {
    std::filesystem::path inputPath;
    std::filesystem::path outputDir;
    std::string baseName;
};

struct TextureExport {
    std::string guid;
    std::string filename;
    bool generateMipmaps = true;
    bool srgb = true;
    bool flipY = true;
};

struct MaterialExport {
    unsigned int materialIndex = 0;
    std::string name;
    std::string guid;
    std::string filename;
    glm::vec3 diffuseColor{1.0f};
    glm::vec3 specularColor{1.0f};
    glm::vec3 emissionColor{0.0f};
    float shininess = 32.0f;
    std::optional<std::string> diffuseTextureGuid;
};

std::string GenerateGuid(const std::string& key);
std::string MakeTextureFilename(const ImportOptions& options,
                                const std::string& alias,
                                const std::string& suffix,
                                const std::string& extension);
void SaveJsonFile(const std::filesystem::path& path, const nlohmann::json& json);


void PrintUsage() {
    fmt::print("Usage: AssimpImporter <model.glb> [--out <output-dir>] [--name <base_name>]\n");
}

std::optional<ImportOptions> ParseArgs(int argc, char** argv) {
    if (argc < 2) {
        PrintUsage();
        return std::nullopt;
    }

    ImportOptions opts;
    opts.inputPath = argv[1];
    if (!std::filesystem::exists(opts.inputPath)) {
        fmt::print(stderr, "Error: input file '{}' does not exist\n", opts.inputPath.string());
        return std::nullopt;
    }

    opts.outputDir = opts.inputPath.parent_path();
    opts.baseName = opts.inputPath.stem().string();

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--out" || arg == "-o") && i + 1 < argc) {
            opts.outputDir = argv[++i];
        } else if ((arg == "--name" || arg == "-n") && i + 1 < argc) {
            opts.baseName = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            PrintUsage();
            return std::nullopt;
        } else {
            fmt::print(stderr, "Unknown argument '{}'\n", arg);
            PrintUsage();
            return std::nullopt;
        }
    }

    if (opts.baseName.empty()) {
        opts.baseName = "ImportedAsset";
    }

    return opts;
}

glm::mat4 ToGlm(const aiMatrix4x4& m) {
    return glm::mat4(
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4
    );
}

glm::vec3 ToGlm(const aiVector3D& v) {
    return glm::vec3(v.x, v.y, v.z);
}

glm::quat ToGlm(const aiQuaternion& q) {
    return glm::quat(q.w, q.x, q.y, q.z);
}

glm::vec3 ToGlm(const aiColor3D& c) {
    return glm::vec3(c.r, c.g, c.b);
}

std::optional<TextureExport> ExportEmbeddedTexture(const aiTexture* texture,
                                                   const ImportOptions& options,
                                                   const std::string& alias,
                                                   const std::string& suffix) {
    if (!texture) {
        return std::nullopt;
    }

    if (texture->mHeight != 0) {
        fmt::print(stderr,
                   "Warning: embedded texture '{}' is uncompressed ({}x{}); skipping export\n",
                   alias,
                   texture->mWidth,
                   texture->mHeight);
        return std::nullopt;
    }

    const std::string hint = texture->achFormatHint[0] ? texture->achFormatHint : "png";
    const std::string filename = MakeTextureFilename(options, alias, suffix, hint);
    const std::filesystem::path outputPath = options.outputDir / filename;

    std::ofstream out(outputPath, std::ios::binary);
    if (!out) {
        fmt::print(stderr, "Warning: failed to write embedded texture '{}'\n", outputPath.string());
        return std::nullopt;
    }

    const std::uint8_t* bytes = reinterpret_cast<const std::uint8_t*>(texture->pcData);
    out.write(reinterpret_cast<const char*>(bytes), static_cast<std::streamsize>(texture->mWidth));

    TextureExport info;
    info.guid = GenerateGuid(filename);
    info.filename = filename;
    return info;
}

std::optional<TextureExport> ExportMaterialTexture(const aiScene* scene,
                                                   const aiMaterial* material,
                                                   aiTextureType type,
                                                   const ImportOptions& options,
                                                   unsigned int materialIndex,
                                                   const std::string& alias,
                                                   const std::string& suffix) {
    if (!material) {
        return std::nullopt;
    }

    aiString texPath;
    if (material->GetTexture(type, 0, &texPath) != AI_SUCCESS) {
        return std::nullopt;
    }
    if (texPath.length == 0) {
        return std::nullopt;
    }

    if (texPath.C_Str()[0] == '*') {
        const aiTexture* embedded = scene->GetEmbeddedTexture(texPath.C_Str());
        return ExportEmbeddedTexture(embedded, options, alias, suffix);
    }

    std::filesystem::path source = texPath.C_Str();
    if (!source.is_absolute()) {
        source = options.inputPath.parent_path() / source;
    }
    if (!std::filesystem::exists(source)) {
        fmt::print(stderr,
                   "Warning: texture '{}' referenced by material {} not found; skipping\n",
                   source.string(),
                   materialIndex);
        return std::nullopt;
    }

    const std::string extension = source.extension().string();
    const std::string filename = MakeTextureFilename(options, alias, suffix, extension);
    const std::filesystem::path destination = options.outputDir / filename;

    std::error_code ec;
    std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        fmt::print(stderr,
                   "Warning: failed to copy texture '{}' to '{}': {}\n",
                   source.string(),
                   destination.string(),
                   ec.message());
        return std::nullopt;
    }

    TextureExport info;
    info.guid = GenerateGuid(filename);
    info.filename = filename;
    return info;
}

std::string SanitizeName(std::string name) {
    if (name.empty()) {
        return "clip";
    }
    std::string result;
    result.reserve(name.size());
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        } else if (c == ' ' || c == '-' || c == '_' || c == '.') {
            result.push_back('_');
        }
    }
    if (result.empty()) {
        result = "clip";
    }
    return result;
}

std::string MakeMaterialAlias(const ImportOptions& options, unsigned int index) {
    return fmt::format("{}_mat{}", SanitizeName(options.baseName), index);
}

std::string MakeMaterialFilename(const ImportOptions& options, unsigned int index) {
    return fmt::format("{}_mat{}.mat", options.baseName, index);
}

std::string MakeTextureFilename(const ImportOptions& options,
                                const std::string& alias,
                                const std::string& suffix,
                                const std::string& extension) {
    std::string sanitizedExt = extension;
    if (!sanitizedExt.empty() && sanitizedExt.front() == '.') {
        sanitizedExt.erase(sanitizedExt.begin());
    }
    return fmt::format("{}_{}_{}.{}", options.baseName, alias, suffix, sanitizedExt.empty() ? "png" : sanitizedExt);
}

std::string GenerateGuid(const std::string& key) {
    constexpr std::uint64_t kOffset = 14695981039346656037ull;
    constexpr std::uint64_t kPrime = 1099511628211ull;

    std::uint64_t hash = kOffset;
    for (unsigned char c : key) {
        hash ^= static_cast<std::uint64_t>(c);
        hash *= kPrime;
    }
    return fmt::format("{:016x}", hash);
}

void CollectBoneNames(const aiScene* scene, std::unordered_set<std::string>& boneNames) {
    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
            const aiBone* bone = mesh->mBones[boneIndex];
            boneNames.insert(bone->mName.C_Str());
        }
    }
}

void CollectInverseBindMatrices(const aiScene* scene,
                                std::unordered_map<std::string, glm::mat4>& inverseBind) {
    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
            const aiBone* bone = mesh->mBones[boneIndex];
            inverseBind[bone->mName.C_Str()] = ToGlm(bone->mOffsetMatrix);
        }
    }
}

void BuildSkeletonRecursive(const aiNode* node,
                            int parentIndex,
                            const std::unordered_set<std::string>& boneNames,
                            const std::unordered_map<std::string, glm::mat4>& inverseBind,
                            Skeleton& skeleton,
                            std::unordered_map<std::string, int>& boneIndices) {
    std::string nodeName = node->mName.C_Str();
    int currentIndex = parentIndex;
    if (boneNames.contains(nodeName)) {
        Skeleton::Bone bone;
        bone.name = nodeName;
        bone.parentIndex = parentIndex;

        if (auto it = inverseBind.find(nodeName); it != inverseBind.end()) {
            bone.inverseBindMatrix = it->second;
        } else {
            bone.inverseBindMatrix = glm::mat4(1.0f);
        }

        currentIndex = static_cast<int>(skeleton.bones.size());
        skeleton.bones.push_back(bone);
        boneIndices[nodeName] = currentIndex;
    }

    for (unsigned int childIdx = 0; childIdx < node->mNumChildren; ++childIdx) {
        BuildSkeletonRecursive(node->mChildren[childIdx],
                               currentIndex,
                               boneNames,
                               inverseBind,
                               skeleton,
                               boneIndices);
    }
}

Skeleton BuildSkeleton(const aiScene* scene,
                       std::unordered_map<std::string, int>& boneIndices) {
    std::unordered_set<std::string> boneNames;
    CollectBoneNames(scene, boneNames);

    std::unordered_map<std::string, glm::mat4> inverseBind;
    CollectInverseBindMatrices(scene, inverseBind);

    Skeleton skeleton;
    skeleton.name = scene->mName.C_Str();
    if (skeleton.name.empty()) {
        skeleton.name = "Skeleton";
    }

    if (scene->mRootNode) {
        BuildSkeletonRecursive(scene->mRootNode, -1, boneNames, inverseBind, skeleton, boneIndices);
    }

    skeleton.bones.shrink_to_fit();
    return skeleton;
}

struct MeshBuildContext {
    SkinnedMeshAsset asset;
    std::vector<std::vector<std::pair<int, float>>> influences;
};

MeshBuildContext BuildSkinnedMesh(const aiScene* scene,
                                  const std::unordered_map<std::string, int>& boneIndices,
                                  const std::map<unsigned int, MaterialExport>& materialExports) {
    MeshBuildContext ctx;
    ctx.asset.name = scene->mName.C_Str();
    if (ctx.asset.name.empty()) {
        ctx.asset.name = "SkinnedMesh";
    }

    std::size_t vertexBase = 0;

    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        if (!mesh->HasBones()) {
            continue;
        }

        const std::size_t meshVertexCount = mesh->mNumVertices;
        ctx.influences.resize(vertexBase + meshVertexCount);

        ctx.asset.vertices.reserve(vertexBase + meshVertexCount);
        for (unsigned int v = 0; v < mesh->mNumVertices; ++v) {
            SkinnedMeshAsset::Vertex vertex;
            if (mesh->HasPositions()) {
                vertex.position = ToGlm(mesh->mVertices[v]);
            }
            if (mesh->HasNormals()) {
                vertex.normal = ToGlm(mesh->mNormals[v]);
            }
            if (mesh->HasTangentsAndBitangents()) {
                vertex.tangent = glm::vec4(mesh->mTangents[v].x,
                                           mesh->mTangents[v].y,
                                           mesh->mTangents[v].z,
                                           1.0f);
            }
            if (mesh->HasTextureCoords(0)) {
                vertex.uv0 = glm::vec2(mesh->mTextureCoords[0][v].x,
                                       mesh->mTextureCoords[0][v].y);
            }
            ctx.asset.vertices.push_back(vertex);
        }

        for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
            const aiBone* bone = mesh->mBones[boneIndex];
            const auto boneIt = boneIndices.find(bone->mName.C_Str());
            if (boneIt == boneIndices.end()) {
                fmt::print(stderr,
                           "Warning: bone '{}' not found in skeleton; skipping weights\n",
                           bone->mName.C_Str());
                continue;
            }
            int boneIdx = boneIt->second;

            for (unsigned int weightIdx = 0; weightIdx < bone->mNumWeights; ++weightIdx) {
                const aiVertexWeight& weight = bone->mWeights[weightIdx];
                std::size_t globalVertex = vertexBase + weight.mVertexId;
                if (globalVertex >= ctx.influences.size()) {
                    continue;
                }
                ctx.influences[globalVertex].emplace_back(boneIdx, weight.mWeight);
            }
        }

        SkinnedMeshAsset::MeshSection section;
        section.materialGuid = "";
        section.indexOffset = static_cast<std::uint32_t>(ctx.asset.indices.size());

        for (unsigned int faceIdx = 0; faceIdx < mesh->mNumFaces; ++faceIdx) {
            const aiFace& face = mesh->mFaces[faceIdx];
            if (face.mNumIndices != 3) {
                continue;
            }
            ctx.asset.indices.push_back(face.mIndices[0] + static_cast<std::uint32_t>(vertexBase));
            ctx.asset.indices.push_back(face.mIndices[1] + static_cast<std::uint32_t>(vertexBase));
            ctx.asset.indices.push_back(face.mIndices[2] + static_cast<std::uint32_t>(vertexBase));
        }

        section.indexCount = static_cast<std::uint32_t>(ctx.asset.indices.size()) - section.indexOffset;
        if (auto materialIt = materialExports.find(mesh->mMaterialIndex); materialIt != materialExports.end()) {
            section.materialGuid = materialIt->second.guid;
        }
        ctx.asset.sections.push_back(section);

        vertexBase += meshVertexCount;
    }

    ctx.asset.boneNames.reserve(boneIndices.size());
    std::vector<std::pair<int, std::string>> ordered;
    ordered.reserve(boneIndices.size());
    for (const auto& [name, index] : boneIndices) {
        ordered.emplace_back(index, name);
    }
    std::sort(ordered.begin(), ordered.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
    for (const auto& [index, name] : ordered) {
        ctx.asset.boneNames.push_back(name);
    }

    ctx.asset.vertices.shrink_to_fit();
    ctx.asset.indices.shrink_to_fit();
    ctx.asset.sections.shrink_to_fit();
    ctx.asset.boneNames.shrink_to_fit();

    return ctx;
}

std::unordered_set<unsigned int> CollectUsedMaterialIndices(const aiScene* scene) {
    std::unordered_set<unsigned int> used;
    if (!scene) {
        return used;
    }
    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        if (!mesh || !mesh->HasBones()) {
            continue;
        }
        used.insert(mesh->mMaterialIndex);
    }
    return used;
}

std::map<unsigned int, MaterialExport> BuildMaterialExports(const aiScene* scene,
                                                            const ImportOptions& options,
                                                            const std::unordered_set<unsigned int>& usedMaterialIndices,
                                                            std::vector<TextureExport>& outTextures) {
    std::map<unsigned int, MaterialExport> exports;
    if (!scene) {
        return exports;
    }

    for (unsigned int materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
        if (!usedMaterialIndices.empty() && !usedMaterialIndices.contains(materialIndex)) {
            continue;
        }

        const aiMaterial* material = scene->mMaterials[materialIndex];
        if (!material) {
            continue;
        }

        MaterialExport info;
        info.materialIndex = materialIndex;
        info.name = MakeMaterialAlias(options, materialIndex);
        info.guid = info.name;
        info.filename = MakeMaterialFilename(options, materialIndex);

        aiColor3D color;
        if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
            info.diffuseColor = ToGlm(color);
        }
        if (material->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS) {
            info.specularColor = ToGlm(color);
        }
        if (material->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS) {
            info.emissionColor = ToGlm(color);
        }
        material->Get(AI_MATKEY_SHININESS, info.shininess);

        if (auto texture = ExportMaterialTexture(scene,
                                                 material,
                                                 aiTextureType_DIFFUSE,
                                                 options,
                                                 materialIndex,
                                                 info.name,
                                                 "diffuse")) {
            outTextures.push_back(*texture);
            info.diffuseTextureGuid = texture->guid;
        }

        exports[materialIndex] = info;
    }

    return exports;
}

void FinalizeVertexWeights(MeshBuildContext& ctx) {
    for (std::size_t i = 0; i < ctx.asset.vertices.size(); ++i) {
        auto& vertex = ctx.asset.vertices[i];
        auto& weights = ctx.influences[i];

        if (weights.empty()) {
            vertex.boneIndices[0] = 0;
            vertex.boneWeights[0] = 1.0f;
            continue;
        }

        std::sort(weights.begin(), weights.end(),
                  [](const auto& lhs, const auto& rhs) { return lhs.second > rhs.second; });

        float total = 0.0f;
        for (std::size_t j = 0; j < weights.size() && j < 4; ++j) {
            vertex.boneIndices[j] = static_cast<std::uint16_t>(weights[j].first);
            vertex.boneWeights[j] = weights[j].second;
            total += weights[j].second;
        }

        if (total > 0.0f) {
            for (float& w : vertex.boneWeights) {
                w = (w > 0.0f) ? (w / total) : 0.0f;
            }
        } else {
            vertex.boneIndices[0] = 0;
            vertex.boneWeights[0] = 1.0f;
        }
    }
}

void WriteMaterialFile(const MaterialExport& material, const ImportOptions& options) {
    nlohmann::json json;
    json["name"] = material.name;
    json["shader"] = "shader::simple_skinned";
    json["diffuseColor"] = {material.diffuseColor.r, material.diffuseColor.g, material.diffuseColor.b};
    json["specularColor"] = {material.specularColor.r, material.specularColor.g, material.specularColor.b};
    json["emissionColor"] = {material.emissionColor.r, material.emissionColor.g, material.emissionColor.b};
    json["shininess"] = material.shininess;
    if (material.diffuseTextureGuid) {
        json["diffuseTexture"] = *material.diffuseTextureGuid;
    }

    const std::filesystem::path path = options.outputDir / material.filename;
    SaveJsonFile(path, json);
}

std::vector<AnimationClip> BuildAnimationClips(const aiScene* scene,
                                               const std::unordered_map<std::string, int>& boneIndices) {
    std::vector<AnimationClip> clips;
    clips.reserve(scene->mNumAnimations);

    for (unsigned int animIndex = 0; animIndex < scene->mNumAnimations; ++animIndex) {
        const aiAnimation* anim = scene->mAnimations[animIndex];
        AnimationClip clip;
        clip.name = anim->mName.C_Str();
        if (clip.name.empty()) {
            clip.name = fmt::format("Animation{}", animIndex);
        }

        clip.duration = anim->mDuration;
        clip.ticksPerSecond = (anim->mTicksPerSecond > 0.0) ? anim->mTicksPerSecond : 25.0;

        clip.channels.reserve(anim->mNumChannels);

        for (unsigned int channelIndex = 0; channelIndex < anim->mNumChannels; ++channelIndex) {
            const aiNodeAnim* channel = anim->mChannels[channelIndex];
            const auto boneIt = boneIndices.find(channel->mNodeName.C_Str());
            if (boneIt == boneIndices.end()) {
                continue;
            }

            AnimationClip::Channel clipChannel;
            clipChannel.boneName = channel->mNodeName.C_Str();
            clipChannel.boneIndex = boneIt->second;

            clipChannel.translationKeys.reserve(channel->mNumPositionKeys);
            for (unsigned int keyIdx = 0; keyIdx < channel->mNumPositionKeys; ++keyIdx) {
                AnimationClip::VecKey key;
                key.time = channel->mPositionKeys[keyIdx].mTime;
                key.value = ToGlm(channel->mPositionKeys[keyIdx].mValue);
                clipChannel.translationKeys.push_back(key);
            }

            clipChannel.rotationKeys.reserve(channel->mNumRotationKeys);
            for (unsigned int keyIdx = 0; keyIdx < channel->mNumRotationKeys; ++keyIdx) {
                AnimationClip::RotKey key;
                key.time = channel->mRotationKeys[keyIdx].mTime;
                key.value = ToGlm(channel->mRotationKeys[keyIdx].mValue);
                clipChannel.rotationKeys.push_back(key);
            }

            clipChannel.scaleKeys.reserve(channel->mNumScalingKeys);
            for (unsigned int keyIdx = 0; keyIdx < channel->mNumScalingKeys; ++keyIdx) {
                AnimationClip::VecKey key;
                key.time = channel->mScalingKeys[keyIdx].mTime;
                key.value = ToGlm(channel->mScalingKeys[keyIdx].mValue);
                clipChannel.scaleKeys.push_back(key);
            }

            clip.channels.push_back(std::move(clipChannel));
        }

        clips.push_back(std::move(clip));
    }

    return clips;
}

void SaveJsonFile(const std::filesystem::path& path, const nlohmann::json& json) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open output file: " + path.string());
    }
    file << json.dump(2);
}

nlohmann::json GeneratePrefab(const ImportOptions& options,
                               const nlohmann::json& manifest,
                               const std::map<unsigned int, MaterialExport>& materialExports) {
    nlohmann::json prefab;
    prefab["name"] = options.baseName;

    nlohmann::json gameObject;
    gameObject["name"] = options.baseName;
    gameObject["active"] = true;
    gameObject["tags"] = nlohmann::json::array({"prop"});

    nlohmann::json components = nlohmann::json::array();

    // TransformComponent
    nlohmann::json transformComp;
    transformComp["type"] = "TransformComponent";
    transformComp["active"] = true;
    transformComp["data"] = {
        {"position", {0.0, 0.0, 0.0}},
        {"rotation", {0.0, 0.0, 0.0}},
        {"scale", {1.0, 1.0, 1.0}}
    };
    components.push_back(transformComp);

    // SkinnedMeshComponent
    nlohmann::json skinnedMeshComp;
    skinnedMeshComp["type"] = "SkinnedMeshComponent";
    skinnedMeshComp["active"] = true;
    nlohmann::json skinnedMeshData;
    
    if (manifest.contains("skinnedMesh") && manifest["skinnedMesh"].contains("guid")) {
        skinnedMeshData["meshGuid"] = manifest["skinnedMesh"]["guid"];
    }
    
    skinnedMeshData["shaderGuid"] = "shader::simple_skinned";
    
    // Get first material GUID if available
    std::string materialGuid;
    std::string textureGuid;
    if (manifest.contains("materials") && manifest["materials"].is_array() && !manifest["materials"].empty()) {
        const auto& firstMat = manifest["materials"][0];
        if (firstMat.contains("guid")) {
            materialGuid = firstMat["guid"].get<std::string>();
            skinnedMeshData["materialGuid"] = materialGuid;
        }
    }
    
    // Try to get texture GUID from first material's diffuse texture
    if (!materialExports.empty()) {
        const auto& firstMaterial = materialExports.begin()->second;
        if (firstMaterial.diffuseTextureGuid) {
            textureGuid = *firstMaterial.diffuseTextureGuid;
            skinnedMeshData["textureGuid"] = textureGuid;
        }
    }
    
    skinnedMeshComp["data"] = skinnedMeshData;
    components.push_back(skinnedMeshComp);

    // AnimatorComponent
    nlohmann::json animatorComp;
    animatorComp["type"] = "AnimatorComponent";
    animatorComp["active"] = true;
    nlohmann::json animatorData;
    
    if (manifest.contains("skeleton") && manifest["skeleton"].contains("guid")) {
        animatorData["skeletonGuid"] = manifest["skeleton"]["guid"];
    }
    
    nlohmann::json layers = nlohmann::json::array();
    if (manifest.contains("animations") && manifest["animations"].is_array()) {
        bool isFirst = true;
        for (const auto& animEntry : manifest["animations"]) {
            nlohmann::json layer;
            if (animEntry.contains("name")) {
                std::string slotName = SanitizeName(animEntry["name"].get<std::string>());
                layer["slot"] = slotName;
            } else {
                layer["slot"] = fmt::format("Layer{}", layers.size());
            }
            
            if (animEntry.contains("guid")) {
                layer["clipGuid"] = animEntry["guid"];
            }
            
            // First animation gets weight 1.0, others get 0.0
            layer["weight"] = isFirst ? 1.0 : 0.0;
            layer["playing"] = true;
            layer["loop"] = true;
            layer["timeSeconds"] = 0.0;
            
            layers.push_back(layer);
            isFirst = false;
        }
    }
    
    animatorData["layers"] = layers;
    animatorComp["data"] = animatorData;
    components.push_back(animatorComp);

    gameObject["components"] = components;
    prefab["gameObjects"] = nlohmann::json::array({gameObject});

    return prefab;
}

} // namespace

int main(int argc, char** argv) {
    auto optionsOpt = ParseArgs(argc, argv);
    if (!optionsOpt.has_value()) {
        return optionsOpt ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    const ImportOptions& options = optionsOpt.value();
    std::filesystem::create_directories(options.outputDir);

    Assimp::Importer importer;
    constexpr unsigned int kFlags =
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_LimitBoneWeights |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace;

    const aiScene* scene = importer.ReadFile(options.inputPath.string(), kFlags);
    if (!scene) {
        fmt::print(stderr, "Assimp failed to load '{}': {}\n", options.inputPath.string(), importer.GetErrorString());
        return EXIT_FAILURE;
    }

    std::unordered_map<std::string, int> boneIndices;
    Skeleton skeleton = BuildSkeleton(scene, boneIndices);

    if (skeleton.bones.empty()) {
        fmt::print(stderr, "Error: no skeleton bones detected in '{}'\n", options.inputPath.string());
        return EXIT_FAILURE;
    }

    const auto usedMaterialIndices = CollectUsedMaterialIndices(scene);
    std::vector<TextureExport> exportedTextures;
    const auto materialExports = BuildMaterialExports(scene, options, usedMaterialIndices, exportedTextures);

    MeshBuildContext meshContext = BuildSkinnedMesh(scene, boneIndices, materialExports);
    FinalizeVertexWeights(meshContext);

    auto clips = BuildAnimationClips(scene, boneIndices);

    const std::filesystem::path skeletonPath = options.outputDir / (options.baseName + ".gmskel");
    const std::filesystem::path skinnedMeshPath = options.outputDir / (options.baseName + ".gmskin");

    try {
        skeleton.SaveToFile(skeletonPath.string());
        meshContext.asset.SaveToFile(skinnedMeshPath.string());
    } catch (const std::exception& ex) {
        fmt::print(stderr, "Failed to save assets: {}\n", ex.what());
        return EXIT_FAILURE;
    }

    std::vector<nlohmann::json> animationEntries;
    animationEntries.reserve(clips.size());

    for (const auto& clip : clips) {
        const std::string sanitized = SanitizeName(clip.name);
        const std::filesystem::path clipPath = options.outputDir / (options.baseName + "_" + sanitized + ".gmanim");
        try {
            clip.SaveToFile(clipPath.string());
        } catch (const std::exception& ex) {
            fmt::print(stderr, "Failed to save animation '{}': {}\n", clip.name, ex.what());
            return EXIT_FAILURE;
        }

        nlohmann::json entry;
        entry["name"] = clip.name;
        entry["guid"] = GenerateGuid(clipPath.filename().string());
        entry["path"] = clipPath.filename().string();
        entry["durationSeconds"] = (clip.ticksPerSecond > 0.0)
                                       ? (clip.duration / clip.ticksPerSecond)
                                       : clip.duration;
        animationEntries.push_back(std::move(entry));
    }

    nlohmann::json texturesJson = nlohmann::json::array();
    std::unordered_set<std::string> seenTextureGuids;
    for (const auto& texture : exportedTextures) {
        if (!seenTextureGuids.insert(texture.guid).second) {
            continue;
        }
        nlohmann::json textureEntry;
        textureEntry["guid"] = texture.guid;
        textureEntry["path"] = texture.filename;
        textureEntry["generateMipmaps"] = texture.generateMipmaps;
        textureEntry["srgb"] = texture.srgb;
        textureEntry["flipY"] = texture.flipY;
        texturesJson.push_back(std::move(textureEntry));
    }

    std::vector<nlohmann::json> materialEntriesJson;
    materialEntriesJson.reserve(materialExports.size());
    for (const auto& [index, material] : materialExports) {
        try {
            WriteMaterialFile(material, options);
        } catch (const std::exception& ex) {
            fmt::print(stderr, "Failed to write material '{}': {}\n", material.filename, ex.what());
            return EXIT_FAILURE;
        }

        nlohmann::json matEntry;
        matEntry["guid"] = material.guid;
        matEntry["path"] = material.filename;
        matEntry["name"] = material.name;
        materialEntriesJson.push_back(std::move(matEntry));
    }

    nlohmann::json manifest;
    manifest["source"] = std::filesystem::absolute(options.inputPath).string();
    manifest["skeleton"] = {
        {"guid", GenerateGuid(skeletonPath.filename().string())},
        {"path", skeletonPath.filename().string()}
    };
    manifest["skinnedMesh"] = {
        {"guid", GenerateGuid(skinnedMeshPath.filename().string())},
        {"path", skinnedMeshPath.filename().string()}
    };
    manifest["animations"] = animationEntries;
    manifest["textures"] = texturesJson;
    manifest["materials"] = materialEntriesJson;

    const std::filesystem::path manifestPath = options.outputDir / (options.baseName + ".animset.json");
    try {
        SaveJsonFile(manifestPath, manifest);
    } catch (const std::exception& ex) {
        fmt::print(stderr, "Failed to save manifest: {}\n", ex.what());
        return EXIT_FAILURE;
    }

    // Generate prefab
    nlohmann::json prefab = GeneratePrefab(options, manifest, materialExports);
    const std::filesystem::path prefabPath = options.outputDir / (options.baseName + ".json");
    try {
        SaveJsonFile(prefabPath, prefab);
        fmt::print("  Prefab      -> {}\n", prefabPath.string());
    } catch (const std::exception& ex) {
        fmt::print(stderr, "Warning: Failed to save prefab: {}\n", ex.what());
        // Don't fail the import if prefab generation fails
    }

    fmt::print("Imported '{}':\n", options.inputPath.string());
    fmt::print("  Skeleton    -> {} ({} bones)\n", skeletonPath.string(), skeleton.bones.size());
    fmt::print("  SkinnedMesh -> {} ({} vertices, {} indices)\n",
               skinnedMeshPath.string(), meshContext.asset.vertices.size(), meshContext.asset.indices.size());
    fmt::print("  Animations  -> {} clips\n", clips.size());
    fmt::print("  Manifest    -> {}\n", manifestPath.string());

    return EXIT_SUCCESS;
}

