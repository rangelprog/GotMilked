#include "GameResources.hpp"
#include "GameConstants.hpp"
#include "GameEvents.hpp"

#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Texture.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/rendering/Material.hpp"
#include "gm/assets/AssetCatalog.hpp"
#include "gm/utils/ResourceManifest.hpp"
#include "gm/utils/ResourceManager.hpp"
#include "gm/utils/ResourceRegistry.hpp"
#include "gm/core/Logger.hpp"
#include "gm/core/Event.hpp"
#include "gm/core/Error.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <exception>
#include <filesystem>
#include <sstream>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <fmt/format.h>
#include <glm/vec3.hpp>

namespace {

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool EndsWith(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() &&
           std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
}

std::filesystem::path ResolveAssetPath(const std::filesystem::path& base, const std::string& input) {
    std::filesystem::path path(input);
    if (path.is_absolute()) {
        return path.lexically_normal();
    }
    return (base / path).lexically_normal();
}

std::string RelativeToAssets(const std::filesystem::path& base, const std::filesystem::path& absolute) {
    std::error_code ec;
    auto relative = std::filesystem::relative(absolute, base, ec);
    if (ec) {
        return absolute.lexically_normal().generic_string();
    }
    return relative.lexically_normal().generic_string();
}

bool IsVertexShaderPath(const std::string& relativeLower) {
    static const std::array<std::string_view, 6> kVertexSuffixes{
        ".vert", ".vert.glsl", ".vs", ".vs.glsl", ".vertex", ".vertex.glsl"
    };
    for (auto suffix : kVertexSuffixes) {
        if (EndsWith(relativeLower, suffix)) {
            return true;
        }
    }
    return false;
}

bool IsFragmentShaderPath(const std::string& relativeLower) {
    static const std::array<std::string_view, 6> kFragmentSuffixes{
        ".frag", ".frag.glsl", ".fs", ".fs.glsl", ".pixel", ".pixel.glsl"
    };
    for (auto suffix : kFragmentSuffixes) {
        if (EndsWith(relativeLower, suffix)) {
            return true;
        }
    }
    return false;
}

std::string ShaderBaseKey(std::string relativeLower) {
    static const std::array<std::string_view, 6> kStageSuffixes{
        ".vert", ".vs", ".vertex", ".frag", ".fs", ".pixel"
    };
    static const std::array<std::string_view, 3> kFormatSuffixes{
        ".glsl", ".hlsl", ".shader"
    };

    for (auto suffix : kFormatSuffixes) {
        if (EndsWith(relativeLower, suffix)) {
            relativeLower.resize(relativeLower.size() - suffix.size());
            break;
        }
    }

    for (auto suffix : kStageSuffixes) {
        if (EndsWith(relativeLower, suffix)) {
            relativeLower.resize(relativeLower.size() - suffix.size());
            break;
        }
    }

    while (!relativeLower.empty() && relativeLower.back() == '.') {
        relativeLower.pop_back();
    }

    return relativeLower;
}

std::uint64_t Fnv1a64(std::string_view data) {
    constexpr std::uint64_t kOffset = 14695981039346656037ull;
    constexpr std::uint64_t kPrime = 1099511628211ull;
    std::uint64_t hash = kOffset;
    for (unsigned char c : data) {
        hash ^= static_cast<std::uint64_t>(c);
        hash *= kPrime;
    }
    return hash;
}

std::string GenerateDeterministicGuid(std::string_view prefix, std::string_view key) {
    auto hash = Fnv1a64(key);
    return fmt::format("{}::{:016x}", prefix, hash);
}

bool FileExists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

struct ShaderFilePair {
    std::optional<gm::assets::AssetDescriptor> vertex;
    std::optional<gm::assets::AssetDescriptor> fragment;
};

std::shared_ptr<gm::core::Error> CloneError(const gm::core::Error& err) {
    if (auto resourceErr = dynamic_cast<const gm::core::ResourceError*>(&err)) {
        return std::make_shared<gm::core::ResourceError>(*resourceErr);
    }
    if (auto graphicsErr = dynamic_cast<const gm::core::GraphicsError*>(&err)) {
        return std::make_shared<gm::core::GraphicsError>(*graphicsErr);
    }
    return std::make_shared<gm::core::Error>(err.what());
}

} // namespace

GameResources::~GameResources() {
    Release();
}

bool GameResources::Load(const std::filesystem::path& assetsDir) {
    Release();
    m_lastError.reset();
    return LoadInternal(assetsDir);
}

bool GameResources::LoadInternal(const std::filesystem::path& assetsDir) {
    auto& registry = gm::ResourceRegistry::Instance();

    std::error_code ec;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(assetsDir, ec);
    if (ec) {
        canonical = std::filesystem::absolute(assetsDir);
    }
    m_assetsDir = canonical;

    auto& catalog = gm::assets::AssetCatalog::Instance();
    catalog.SetAssetRoot(m_assetsDir);
    catalog.Scan();

    const auto catalogAssets = catalog.GetAllAssets();

    std::unordered_map<std::string, gm::assets::AssetDescriptor> meshAssets;
    std::unordered_map<std::string, ShaderFilePair> shaderFilePairs;

    auto isUnderDirectory = [](const std::string& relative, std::string_view directory) {
        if (relative.size() < directory.size()) {
            return false;
        }
        return std::equal(directory.begin(), directory.end(), relative.begin());
    };

    for (const auto& asset : catalogAssets) {
        switch (asset.type) {
        case gm::assets::AssetType::Mesh:
            if (isUnderDirectory(asset.relativePath, "models/")) {
                meshAssets.emplace(asset.guid, asset);
            }
            break;
        case gm::assets::AssetType::Shader: {
            const auto relativeLower = ToLower(asset.relativePath);
            if (!isUnderDirectory(asset.relativePath, "shaders/")) {
                break;
            }
            auto baseKey = ShaderBaseKey(relativeLower);
            auto& pair = shaderFilePairs[baseKey];
            if (IsVertexShaderPath(relativeLower)) {
                pair.vertex = asset;
            } else if (IsFragmentShaderPath(relativeLower)) {
                pair.fragment = asset;
            }
            break;
        }
        case gm::assets::AssetType::Prefab:
            if (isUnderDirectory(asset.relativePath, "prefabs/")) {
                m_prefabSources[asset.guid] = asset.relativePath;
            }
            break;
        default:
            break;
        }
    }

    std::unordered_set<std::string> loadedShaderGuids;
    std::unordered_set<std::string> loadedMeshGuids;
    std::unordered_set<std::string> consumedShaderBases;

    m_prefabSources.clear();

    bool success = false;

    try {
        for (const auto& [baseKey, files] : shaderFilePairs) {
            if (baseKey.empty()) {
                continue;
            }
            if (consumedShaderBases.count(baseKey)) {
                continue;
            }
            if (!files.vertex || !files.fragment) {
                continue;
            }
            if (!isUnderDirectory(files.vertex->relativePath, "shaders/") ||
                !isUnderDirectory(files.fragment->relativePath, "shaders/")) {
                continue;
            }

            auto vertPath = (m_assetsDir / files.vertex->relativePath).lexically_normal();
            auto fragPath = (m_assetsDir / files.fragment->relativePath).lexically_normal();

            if (!FileExists(vertPath) || !FileExists(fragPath)) {
                gm::core::Logger::Warning("[GameResources] Catalog shader '{}' missing files ({} / {})",
                                          baseKey,
                                          vertPath.generic_string(),
                                          fragPath.generic_string());
                continue;
            }

            std::string guid = files.vertex->guid;
            if (loadedShaderGuids.count(guid) || m_shaders.count(guid)) {
                guid = GenerateDeterministicGuid("shader", baseKey);
            }
            if (loadedShaderGuids.count(guid) || m_shaders.count(guid)) {
                guid = GenerateDeterministicGuid("shader_auto", baseKey);
            }

            gm::ResourceManager::ShaderDescriptor descriptor{
                guid,
                vertPath.string(),
                fragPath.string()
            };
            auto handle = gm::ResourceManager::LoadShader(descriptor);
            auto shader = handle.Lock();
            if (!shader) {
                throw gm::core::ResourceError("shader", guid, "Loaded shader handle is empty");
            }
            shader->Use();
            shader->SetInt("uTex", 0);

            m_shaders[guid] = shader;
            m_shaderSources[guid] = ShaderSources{descriptor.vertexPath, descriptor.fragmentPath};
            registry.RegisterShader(guid, descriptor.vertexPath, descriptor.fragmentPath);
            loadedShaderGuids.insert(guid);
            consumedShaderBases.insert(baseKey);
        }

        for (const auto& [guid, descriptor] : meshAssets) {
            if (loadedMeshGuids.count(guid)) {
                continue;
            }
            if (!isUnderDirectory(descriptor.relativePath, "models/")) {
                continue;
            }

            auto path = (m_assetsDir / descriptor.relativePath).lexically_normal();
            if (!FileExists(path)) {
                gm::core::Logger::Warning("[GameResources] Catalog mesh '{}' missing file '{}'",
                                          guid,
                                          path.generic_string());
                continue;
            }

            gm::ResourceManager::MeshDescriptor desc{
                guid,
                path.string()
            };

            auto handle = gm::ResourceManager::LoadMesh(desc);
            auto mesh = handle.Lock();
            if (!mesh) {
                throw gm::core::ResourceError("mesh", guid, "Loaded mesh handle is empty");
            }

            gm::utils::ResourceManifest::MeshEntry entry;
            entry.guid = guid;
            entry.path = desc.path;

            m_meshes[guid] = mesh;
            m_meshSources[guid] = entry;
            registry.RegisterMesh(guid, entry.path);
            loadedMeshGuids.insert(guid);
        }

        RegisterDefaults();

        if (auto shader = GetDefaultShader()) {
            shader->Use();
            shader->SetInt("uTex", 0);
            gm::core::Event::Trigger(gotmilked::GameEvents::ResourceShaderLoaded);
        }

        if (GetDefaultMesh()) {
            gm::core::Event::Trigger(gotmilked::GameEvents::ResourceMeshLoaded);
        }
        success = true;
    } catch (const gm::core::Error& err) {
        StoreError(err);
        gm::core::Logger::Error("[GameResources] {}", err.what());
        gm::core::Event::Trigger(gotmilked::GameEvents::ResourceLoadFailed);
        gm::core::Event::TriggerWithData(gotmilked::GameEvents::ResourceLoadFailed, m_lastError.get());
    } catch (const std::exception& ex) {
        gm::core::Error err(std::string("GameResources load failed: ") + ex.what());
        StoreError(err);
        gm::core::Logger::Error("[GameResources] {}", err.what());
        gm::core::Event::Trigger(gotmilked::GameEvents::ResourceLoadFailed);
        gm::core::Event::TriggerWithData(gotmilked::GameEvents::ResourceLoadFailed, m_lastError.get());
    }

    RegisterCatalogListener();
    catalog.StartWatching();

    return success;
}

void GameResources::RegisterCatalogListener() {
    if (m_catalogListener != 0) {
        return;
    }

    auto& catalog = gm::assets::AssetCatalog::Instance();
    m_catalogListener = catalog.RegisterListener([this](const gm::assets::AssetEvent& event) {
        std::lock_guard<std::mutex> lock(m_catalogEventMutex);
        m_catalogEvents.push_back(event);
        m_catalogDirty.store(true, std::memory_order_relaxed);
    });
}

void GameResources::UnregisterCatalogListener() {
    auto& catalog = gm::assets::AssetCatalog::Instance();
    if (m_catalogListener != 0) {
        catalog.UnregisterListener(m_catalogListener);
        m_catalogListener = 0;
    }

    {
        std::lock_guard<std::mutex> lock(m_catalogEventMutex);
        m_catalogEvents.clear();
    }
    m_catalogDirty.store(false, std::memory_order_relaxed);
}

GameResources::CatalogUpdateResult GameResources::ProcessCatalogEvents() {
    CatalogUpdateResult result{};

    if (!m_catalogDirty.load(std::memory_order_relaxed)) {
        return result;
    }

    std::vector<gm::assets::AssetEvent> events;
    {
        std::lock_guard<std::mutex> lock(m_catalogEventMutex);
        if (m_catalogEvents.empty()) {
            m_catalogDirty.store(false, std::memory_order_relaxed);
            return result;
        }
        events.swap(m_catalogEvents);
    }
    m_catalogDirty.store(false, std::memory_order_relaxed);

    result.hadEvents = !events.empty();
    if (!result.hadEvents) {
        return result;
    }

    result.prefabsChanged = std::any_of(events.begin(), events.end(), [](const gm::assets::AssetEvent& evt) {
        return evt.descriptor.relativePath.rfind("prefabs/", 0) == 0;
    });

    if (m_assetsDir.empty()) {
        gm::core::Logger::Warning("[GameResources] Catalog events detected but assets directory is unset");
        return result;
    }

    gm::core::Logger::Info("[GameResources] Processing {} asset catalog event(s); rebuilding resources", events.size());

    Release();

    result.reloadSucceeded = LoadInternal(m_assetsDir);
    if (!result.reloadSucceeded) {
        gm::core::Logger::Error("[GameResources] Failed to rebuild resources after catalog change");
    } else {
        gm::core::Logger::Info("[GameResources] Resources rebuilt successfully after catalog change");
    }

    return result;
}

void GameResources::RegisterDefaults() {
    auto chooseGuid = [](const auto& map) -> std::string {
        if (!map.empty()) {
            return map.begin()->first;
        }
        return {};
    };

    m_defaultShaderGuid = chooseGuid(m_shaders);
    if (auto it = m_shaderSources.find(m_defaultShaderGuid); it != m_shaderSources.end()) {
        m_defaultShaderVertPath = it->second.vertPath;
        m_defaultShaderFragPath = it->second.fragPath;
    } else {
        m_defaultShaderVertPath.clear();
        m_defaultShaderFragPath.clear();
    }

    m_defaultTextureGuid.clear();
    m_defaultTexturePath.clear();

    m_defaultMeshGuid = chooseGuid(m_meshes);
    if (auto it = m_meshSources.find(m_defaultMeshGuid); it != m_meshSources.end()) {
        m_defaultMeshPath = it->second.path;
    } else {
        m_defaultMeshPath.clear();
    }

    m_defaultTerrainMaterialGuid.clear();
}

void GameResources::EnsureTextureRegistered(const std::string& guid, std::shared_ptr<gm::Texture> texture) {
    if (guid.empty() || !texture) {
        return;
    }

    m_textures[guid] = texture;

    if (!m_textureSources.contains(guid)) {
        gm::utils::ResourceManifest::TextureEntry entry;
        entry.guid = guid;
        entry.generateMipmaps = true;
        entry.srgb = true;
        entry.flipY = true;
        if (!m_assetsDir.empty()) {
            if (auto descriptor = gm::assets::AssetCatalog::Instance().FindByGuid(guid)) {
                if (!descriptor->relativePath.empty()) {
                    entry.path = (m_assetsDir / descriptor->relativePath).lexically_normal().string();
                }
            }
        }
        m_textureSources[guid] = entry;
        if (!entry.path.empty()) {
            gm::ResourceRegistry::Instance().RegisterTexture(guid, entry.path);
        }
    }
}

void GameResources::StoreError(const gm::core::Error& err) {
    m_lastError = CloneError(err);
}

gm::Shader* GameResources::GetShader(const std::string& guid) const {
    auto it = m_shaders.find(guid);
    return it != m_shaders.end() ? it->second.get() : nullptr;
}

gm::Texture* GameResources::GetTexture(const std::string& guid) const {
    auto it = m_textures.find(guid);
    return it != m_textures.end() ? it->second.get() : nullptr;
}

gm::Mesh* GameResources::GetMesh(const std::string& guid) const {
    auto it = m_meshes.find(guid);
    return it != m_meshes.end() ? it->second.get() : nullptr;
}

std::shared_ptr<gm::Material> GameResources::GetMaterial(const std::string& guid) const {
    auto it = m_materials.find(guid);
    if (it != m_materials.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<gm::Texture> GameResources::GetTextureShared(const std::string& guid) const {
    auto it = m_textures.find(guid);
    if (it != m_textures.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<gm::Texture> GameResources::EnsureTextureAvailable(const std::string& guid) {
    if (guid.empty()) {
        return nullptr;
    }

    if (auto it = m_textures.find(guid); it != m_textures.end()) {
        return it->second;
    }

    auto descriptorOpt = gm::assets::AssetCatalog::Instance().FindByGuid(guid);
    if (!descriptorOpt) {
        return nullptr;
    }

    gm::ResourceManager::TextureDescriptor desc;
    desc.guid = guid;
    desc.path = (m_assetsDir / descriptorOpt->relativePath).lexically_normal().string();
    desc.generateMipmaps = true;
    desc.srgb = true;
    desc.flipY = true;

    auto handle = gm::ResourceManager::LoadTexture(desc);
    auto textureShared = handle.Lock();
    if (!textureShared) {
        return nullptr;
    }

    m_textures[guid] = textureShared;

    gm::utils::ResourceManifest::TextureEntry entry;
    entry.guid = guid;
    entry.path = desc.path;
    entry.generateMipmaps = desc.generateMipmaps;
    entry.srgb = desc.srgb;
    entry.flipY = desc.flipY;
    m_textureSources[guid] = entry;

    gm::ResourceRegistry::Instance().RegisterTexture(guid, entry.path);
    return textureShared;
}

gm::Shader* GameResources::GetDefaultShader() const {
    return GetShader(m_defaultShaderGuid);
}

gm::Texture* GameResources::GetDefaultTexture() const {
    return GetTexture(m_defaultTextureGuid);
}

gm::Mesh* GameResources::GetDefaultMesh() const {
    return GetMesh(m_defaultMeshGuid);
}

std::shared_ptr<gm::Material> GameResources::GetTerrainMaterial() const {
    return GetMaterial(m_defaultTerrainMaterialGuid);
}

std::optional<gm::utils::ResourceManifest::ShaderEntry> GameResources::GetShaderSource(const std::string& guid) const {
    if (auto it = m_shaderSources.find(guid); it != m_shaderSources.end()) {
        gm::utils::ResourceManifest::ShaderEntry entry;
        entry.guid = guid;
        entry.vertexPath = it->second.vertPath;
        entry.fragmentPath = it->second.fragPath;
        return entry;
    }
    return std::nullopt;
}

std::optional<std::string> GameResources::GetTextureSource(const std::string& guid) const {
    if (auto it = m_textureSources.find(guid); it != m_textureSources.end()) {
        return it->second.path;
    }
    return std::nullopt;
}

std::optional<std::string> GameResources::GetMeshSource(const std::string& guid) const {
    if (auto it = m_meshSources.find(guid); it != m_meshSources.end()) {
        return it->second.path;
    }
    return std::nullopt;
}

bool GameResources::ReloadShader() {
    if (m_defaultShaderGuid.empty()) {
        gm::core::Error err("GameResources: shader GUID not set");
        StoreError(err);
        gm::core::Logger::Warning("[GameResources] Cannot reload shader: GUID not set");
        return false;
    }
    return ReloadShader(m_defaultShaderGuid);
}

bool GameResources::ReloadShader(const std::string& guid) {
    auto it = m_shaderSources.find(guid);
    if (it == m_shaderSources.end()) {
        gm::core::Error err("GameResources: shader GUID not recognized");
        StoreError(err);
        gm::core::Logger::Warning("[GameResources] Cannot reload shader: GUID '{}' not known", guid);
        return false;
    }

    try {
        gm::ResourceManager::ShaderDescriptor descriptor{
            guid,
            it->second.vertPath,
            it->second.fragPath
        };
        auto handle = gm::ResourceManager::ReloadShader(descriptor);
        auto shader = handle.Lock();
        if (!shader) {
            throw gm::core::ResourceError("shader", guid, "Reloaded shader handle is empty");
        }
        shader->Use();
        shader->SetInt("uTex", 0);
        m_shaders[guid] = shader;
        gm::ResourceRegistry::Instance().RegisterShader(guid, it->second.vertPath, it->second.fragPath);
        gm::core::Event::Trigger(gotmilked::GameEvents::ResourceShaderReloaded);
        return true;
    } catch (const gm::core::Error& err) {
        StoreError(err);
        gm::core::Logger::Error("[GameResources] {}", err.what());
    } catch (const std::exception& ex) {
        gm::core::Error err(std::string("GameResources shader reload failed: ") + ex.what());
        StoreError(err);
        gm::core::Logger::Error("[GameResources] {}", err.what());
    }

    gm::core::Event::Trigger(gotmilked::GameEvents::ResourceLoadFailed);
    gm::core::Event::TriggerWithData(gotmilked::GameEvents::ResourceLoadFailed, m_lastError.get());
    return false;
}

bool GameResources::ReloadTexture() {
    if (m_defaultTextureGuid.empty()) {
        gm::core::Error err("GameResources: texture GUID not set");
        StoreError(err);
        gm::core::Logger::Warning("[GameResources] Cannot reload texture: GUID not set");
        return false;
    }
    return ReloadTexture(m_defaultTextureGuid);
}

bool GameResources::ReloadTexture(const std::string& guid) {
    auto it = m_textureSources.find(guid);
    if (it == m_textureSources.end() || it->second.path.empty()) {
        gm::core::Error err("GameResources: texture path not set");
        StoreError(err);
        gm::core::Logger::Warning("[GameResources] Cannot reload texture: path not set for GUID '{}'", guid);
        return false;
    }

    try {
        gm::ResourceManager::TextureDescriptor descriptor{
            guid,
            it->second.path,
            it->second.generateMipmaps,
            it->second.srgb,
            it->second.flipY
        };
        auto handle = gm::ResourceManager::ReloadTexture(descriptor);
        auto texture = handle.Lock();
        if (!texture) {
            throw gm::core::ResourceError("texture", guid, "Reloaded texture handle is empty");
        }
        m_textures[guid] = texture;

        for (const auto& [materialGuid, materialEntry] : m_materialSources) {
            auto materialIt = m_materials.find(materialGuid);
            if (materialIt == m_materials.end()) {
                continue;
            }

            auto& material = materialIt->second;
            if (materialEntry.diffuseTextureGuid && *materialEntry.diffuseTextureGuid == guid) {
                material->SetDiffuseTexture(texture.get());
            }
            if (materialEntry.specularTextureGuid && *materialEntry.specularTextureGuid == guid) {
                material->SetSpecularTexture(texture.get());
            }
            if (materialEntry.normalTextureGuid && *materialEntry.normalTextureGuid == guid) {
                material->SetNormalTexture(texture.get());
            }
            if (materialEntry.emissionTextureGuid && *materialEntry.emissionTextureGuid == guid) {
                material->SetEmissionTexture(texture.get());
            }
        }

        gm::ResourceRegistry::Instance().RegisterTexture(guid, it->second.path);
        gm::core::Event::Trigger(gotmilked::GameEvents::ResourceTextureReloaded);
        return true;
    } catch (const gm::core::Error& err) {
        StoreError(err);
        gm::core::Logger::Error("[GameResources] {}", err.what());
    } catch (const std::exception& ex) {
        gm::core::Error err(std::string("GameResources texture reload failed: ") + ex.what());
        StoreError(err);
        gm::core::Logger::Error("[GameResources] {}", err.what());
    }

    gm::core::Event::Trigger(gotmilked::GameEvents::ResourceLoadFailed);
    gm::core::Event::TriggerWithData(gotmilked::GameEvents::ResourceLoadFailed, m_lastError.get());
    return false;
}

bool GameResources::ReloadMesh() {
    if (m_defaultMeshGuid.empty()) {
        gm::core::Error err("GameResources: mesh GUID not set");
        StoreError(err);
        gm::core::Logger::Warning("[GameResources] Cannot reload mesh: GUID not set");
        return false;
    }
    return ReloadMesh(m_defaultMeshGuid);
}

bool GameResources::ReloadMesh(const std::string& guid) {
    auto it = m_meshSources.find(guid);
    if (it == m_meshSources.end() || it->second.path.empty()) {
        gm::core::Error err("GameResources: mesh path not set");
        StoreError(err);
        gm::core::Logger::Warning("[GameResources] Cannot reload mesh: path not set for GUID '{}'", guid);
        return false;
    }

    try {
        gm::ResourceManager::MeshDescriptor descriptor{
            guid,
            it->second.path
        };
        auto handle = gm::ResourceManager::ReloadMesh(descriptor);
        auto mesh = handle.Lock();
        if (!mesh) {
            throw gm::core::ResourceError("mesh", guid, "Reloaded mesh handle is empty");
        }
        m_meshes[guid] = mesh;
        gm::ResourceRegistry::Instance().RegisterMesh(guid, it->second.path);
        gm::core::Event::Trigger(gotmilked::GameEvents::ResourceMeshReloaded);
        return true;
    } catch (const gm::core::Error& err) {
        StoreError(err);
        gm::core::Logger::Error("[GameResources] {}", err.what());
    } catch (const std::exception& ex) {
        gm::core::Error err(std::string("GameResources mesh reload failed: ") + ex.what());
        StoreError(err);
        gm::core::Logger::Error("[GameResources] {}", err.what());
    }

    gm::core::Event::Trigger(gotmilked::GameEvents::ResourceLoadFailed);
    gm::core::Event::TriggerWithData(gotmilked::GameEvents::ResourceLoadFailed, m_lastError.get());
    return false;
}

bool GameResources::ReloadAll() {
    bool shaderOk = ReloadShader();
    bool textureOk = ReloadTexture();
    bool meshOk = ReloadMesh();

    bool allOk = shaderOk && textureOk && meshOk;
    if (allOk) {
        gm::core::Logger::Info("[GameResources] ReloadAll: all resources reloaded successfully");
        gm::core::Event::Trigger(gotmilked::GameEvents::ResourceAllReloaded);
    }

    return allOk;
}

void GameResources::Release() {
    auto& catalog = gm::assets::AssetCatalog::Instance();
    catalog.StopWatching();
    UnregisterCatalogListener();

    auto& registry = gm::ResourceRegistry::Instance();
    for (const auto& [guid, _] : m_shaders) {
        registry.UnregisterShader(guid);
    }
    for (const auto& [guid, _] : m_textures) {
        registry.UnregisterTexture(guid);
    }
    for (const auto& [guid, _] : m_meshes) {
        registry.UnregisterMesh(guid);
    }
    for (const auto& [guid, _] : m_materials) {
        registry.UnregisterMaterial(guid);
    }

    m_shaders.clear();
    m_textures.clear();
    m_meshes.clear();
    m_materials.clear();
    m_shaderSources.clear();
    m_textureSources.clear();
    m_meshSources.clear();
    m_materialSources.clear();
    m_prefabSources.clear();

    m_defaultShaderGuid.clear();
    m_defaultShaderVertPath.clear();
    m_defaultShaderFragPath.clear();
    m_defaultTextureGuid.clear();
    m_defaultTexturePath.clear();
    m_defaultMeshGuid.clear();
    m_defaultMeshPath.clear();
    m_defaultTerrainMaterialGuid.clear();

    m_lastError.reset();
}

