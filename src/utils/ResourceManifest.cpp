#include "gm/utils/ResourceManifest.hpp"

#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>
#include <glm/vec3.hpp>

namespace gm::utils {
namespace {

glm::vec3 ParseVec3(const nlohmann::json& node, const glm::vec3& fallback, std::vector<std::string>& warnings, const std::string& context) {
    if (!node.is_array()) {
        warnings.push_back(context + ": expected array for color, using fallback");
        return fallback;
    }
    if (node.size() != 3) {
        warnings.push_back(context + ": expected array of size 3 for color, using fallback");
        return fallback;
    }

    glm::vec3 value = fallback;
    for (std::size_t i = 0; i < 3; ++i) {
        if (!node[i].is_number()) {
            warnings.push_back(context + ": color element at index " + std::to_string(i) + " is not a number, using fallback");
            return fallback;
        }
        value[static_cast<int>(i)] = node[i].get<float>();
    }
    return value;
}

template <typename T>
bool RequireString(const nlohmann::json& json, const char* key, std::string& out, std::vector<std::string>& errors, const T& context) {
    if (!json.contains(key)) {
        errors.push_back(context + ": missing required key '" + std::string(key) + "'");
        return false;
    }
    if (!json[key].is_string()) {
        errors.push_back(context + ": expected key '" + std::string(key) + "' to be a string");
        return false;
    }
    out = json[key].get<std::string>();
    return true;
}

} // namespace

ResourceManifestLoadResult LoadResourceManifest(const std::filesystem::path& manifestPath) {
    ResourceManifestLoadResult result;

    std::ifstream file(manifestPath);
    if (!file.is_open()) {
        result.errors.push_back("Failed to open resource manifest: " + manifestPath.string());
        return result;
    }

    nlohmann::json json;
    try {
        file >> json;
    } catch (const std::exception& ex) {
        result.errors.push_back(std::string("Failed to parse resource manifest JSON: ") + ex.what());
        return result;
    }

    const auto addContext = [](const std::string& base, const std::string& guid) {
        if (guid.empty()) {
            return base;
        }
        return base + " '" + guid + "'";
    };

    if (json.contains("shaders")) {
        if (!json["shaders"].is_object()) {
            result.errors.push_back("'shaders' section must be an object mapping GUID to shader definition");
        } else {
            for (const auto& [guid, shaderJson] : json["shaders"].items()) {
                if (!shaderJson.is_object()) {
                    result.errors.push_back(addContext("Shader entry must be an object for GUID", guid));
                    continue;
                }

                ResourceManifest::ShaderEntry entry;
                entry.guid = guid;

                std::string context = addContext("Shader", guid);
                bool ok = RequireString(shaderJson, "vertex", entry.vertexPath, result.errors, context);
                ok = RequireString(shaderJson, "fragment", entry.fragmentPath, result.errors, context) && ok;

                if (ok) {
                    result.manifest.shaders.push_back(std::move(entry));
                }
            }
        }
    }

    if (json.contains("textures")) {
        if (!json["textures"].is_object()) {
            result.errors.push_back("'textures' section must be an object mapping GUID to texture definition");
        } else {
            for (const auto& [guid, textureJson] : json["textures"].items()) {
                if (!textureJson.is_object()) {
                    result.errors.push_back(addContext("Texture entry must be an object for GUID", guid));
                    continue;
                }

                ResourceManifest::TextureEntry entry;
                entry.guid = guid;
                std::string context = addContext("Texture", guid);

                if (!RequireString(textureJson, "path", entry.path, result.errors, context)) {
                    continue;
                }

                if (textureJson.contains("generateMipmaps")) {
                    entry.generateMipmaps = textureJson["generateMipmaps"].get<bool>();
                }
                if (textureJson.contains("srgb")) {
                    entry.srgb = textureJson["srgb"].get<bool>();
                }
                if (textureJson.contains("flipY")) {
                    entry.flipY = textureJson["flipY"].get<bool>();
                }

                result.manifest.textures.push_back(std::move(entry));
            }
        }
    }

    if (json.contains("meshes")) {
        if (!json["meshes"].is_object()) {
            result.errors.push_back("'meshes' section must be an object mapping GUID to mesh definition");
        } else {
            for (const auto& [guid, meshJson] : json["meshes"].items()) {
                if (!meshJson.is_object()) {
                    result.errors.push_back(addContext("Mesh entry must be an object for GUID", guid));
                    continue;
                }

                ResourceManifest::MeshEntry entry;
                entry.guid = guid;
                std::string context = addContext("Mesh", guid);

                if (!RequireString(meshJson, "path", entry.path, result.errors, context)) {
                    continue;
                }

                result.manifest.meshes.push_back(std::move(entry));
            }
        }
    }

    if (json.contains("materials")) {
        if (!json["materials"].is_object()) {
            result.errors.push_back("'materials' section must be an object mapping GUID to material definition");
        } else {
            for (const auto& [guid, materialJson] : json["materials"].items()) {
                if (!materialJson.is_object()) {
                    result.errors.push_back(addContext("Material entry must be an object for GUID", guid));
                    continue;
                }

                ResourceManifest::MaterialEntry entry;
                entry.guid = guid;
                entry.name = guid;
                std::string context = addContext("Material", guid);

                if (materialJson.contains("name") && materialJson["name"].is_string()) {
                    entry.name = materialJson["name"].get<std::string>();
                }
                if (materialJson.contains("diffuseColor")) {
                    entry.diffuseColor = ParseVec3(materialJson["diffuseColor"], entry.diffuseColor, result.warnings, context + " diffuseColor");
                }
                if (materialJson.contains("specularColor")) {
                    entry.specularColor = ParseVec3(materialJson["specularColor"], entry.specularColor, result.warnings, context + " specularColor");
                }
                if (materialJson.contains("emissionColor")) {
                    entry.emissionColor = ParseVec3(materialJson["emissionColor"], entry.emissionColor, result.warnings, context + " emissionColor");
                }
                if (materialJson.contains("shininess")) {
                    if (materialJson["shininess"].is_number()) {
                        entry.shininess = materialJson["shininess"].get<float>();
                    } else {
                        result.warnings.push_back(context + ": shininess must be numeric, using default");
                    }
                }
                if (materialJson.contains("diffuseTexture") && materialJson["diffuseTexture"].is_string()) {
                    entry.diffuseTextureGuid = materialJson["diffuseTexture"].get<std::string>();
                }
                if (materialJson.contains("specularTexture") && materialJson["specularTexture"].is_string()) {
                    entry.specularTextureGuid = materialJson["specularTexture"].get<std::string>();
                }
                if (materialJson.contains("normalTexture") && materialJson["normalTexture"].is_string()) {
                    entry.normalTextureGuid = materialJson["normalTexture"].get<std::string>();
                }
                if (materialJson.contains("emissionTexture") && materialJson["emissionTexture"].is_string()) {
                    entry.emissionTextureGuid = materialJson["emissionTexture"].get<std::string>();
                }

                result.manifest.materials.push_back(std::move(entry));
            }
        }
    }

    if (json.contains("defaults")) {
        if (!json["defaults"].is_object()) {
            result.errors.push_back("'defaults' section must be an object");
        } else {
            const auto& defaults = json["defaults"];
            if (defaults.contains("shader") && defaults["shader"].is_string()) {
                result.manifest.defaults.shaderGuid = defaults["shader"].get<std::string>();
            }
            if (defaults.contains("texture") && defaults["texture"].is_string()) {
                result.manifest.defaults.textureGuid = defaults["texture"].get<std::string>();
            }
            if (defaults.contains("mesh") && defaults["mesh"].is_string()) {
                result.manifest.defaults.meshGuid = defaults["mesh"].get<std::string>();
            }
            if (defaults.contains("terrainMaterial") && defaults["terrainMaterial"].is_string()) {
                result.manifest.defaults.terrainMaterialGuid = defaults["terrainMaterial"].get<std::string>();
            }
        }
    }

    result.success = result.errors.empty();
    return result;
}

} // namespace gm::utils


