#include "gm/assets/AssetDatabase.hpp"

#include "gm/core/Logger.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fmt/format.h>

namespace gm::assets {

namespace {

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

bool IsUnderDirectory(std::string_view relative, std::string_view directory) {
    if (relative.size() < directory.size()) {
        return false;
    }
    return std::equal(directory.begin(), directory.end(), relative.begin());
}

} // namespace

AssetDatabase& AssetDatabase::Instance() {
    static AssetDatabase instance;
    return instance;
}

AssetDatabase::~AssetDatabase() {
    Shutdown();
}

void AssetDatabase::Initialize(const std::filesystem::path& assetRoot) {
    std::filesystem::path canonical;
    {
        std::lock_guard stateLock(m_stateMutex);
        std::error_code ec;
        canonical = std::filesystem::weakly_canonical(assetRoot, ec);
        if (ec) {
            canonical = std::filesystem::absolute(assetRoot);
        }

        if (m_initialized.load(std::memory_order_acquire)) {
            if (canonical == m_assetRoot) {
                return;
            }
            StopThreads();
            m_catalogListener = 0;
            m_initialized.store(false, std::memory_order_release);
        }

        m_assetRoot = std::move(canonical);
        m_stopRequested = false;
        m_dirty = true;
        m_ready.store(false, std::memory_order_release);
        m_indexVersion.store(0, std::memory_order_release);
    }

    auto& catalog = AssetCatalog::Instance();
    catalog.SetAssetRoot(m_assetRoot);
    catalog.Scan();

    {
        std::unique_lock listenerLock(m_listenerMutex);
        if (m_catalogListener != 0) {
            catalog.UnregisterListener(m_catalogListener);
            m_catalogListener = 0;
        }
        m_catalogListener = catalog.RegisterListener([this](const AssetEvent& event) {
            HandleAssetEvent(event);
        });
    }

    catalog.StartWatching();

    StartThreads();
    m_initialized.store(true, std::memory_order_release);
}

void AssetDatabase::Shutdown() {
    {
        std::lock_guard stateLock(m_stateMutex);
        if (!m_initialized.load(std::memory_order_acquire)) {
            return;
        }
        m_stopRequested = true;
    }

    StopThreads();

    auto& catalog = AssetCatalog::Instance();
    if (m_catalogListener != 0) {
        catalog.UnregisterListener(m_catalogListener);
        m_catalogListener = 0;
    }
    catalog.StopWatching();

    {
        std::unique_lock cacheLock(m_cacheMutex);
        m_shaderBatches.clear();
        m_meshRecords.clear();
        m_prefabRecords.clear();
        m_descriptorsByGuid.clear();
    }

    m_initialized.store(false, std::memory_order_release);
    m_ready.store(false, std::memory_order_release);
}

std::filesystem::path AssetDatabase::AssetRoot() const {
    std::shared_lock lock(m_cacheMutex);
    return m_assetRoot;
}

void AssetDatabase::WaitForInitialIndex() {
    std::unique_lock lock(m_conditionMutex);
    m_condition.wait(lock, [this]() { return m_ready.load(std::memory_order_acquire) || m_stopRequested; });
}

void AssetDatabase::WaitUntilIdle() {
    std::unique_lock lock(m_conditionMutex);
    m_condition.wait(lock, [this]() { return (m_ready.load(std::memory_order_acquire) && !m_dirty && !m_indexInProgress) || m_stopRequested; });
}

std::vector<AssetDatabase::ShaderBatchRecord> AssetDatabase::GetShaderBatches() const {
    std::shared_lock lock(m_cacheMutex);
    return m_shaderBatches;
}

std::vector<AssetDatabase::MeshRecord> AssetDatabase::GetMeshRecords() const {
    std::shared_lock lock(m_cacheMutex);
    return m_meshRecords;
}

std::vector<AssetDatabase::PrefabRecord> AssetDatabase::GetPrefabRecords() const {
    std::shared_lock lock(m_cacheMutex);
    return m_prefabRecords;
}

std::vector<AssetDatabase::ManifestRecord> AssetDatabase::GetManifestRecords() const {
    std::shared_lock lock(m_cacheMutex);
    return m_manifestRecords;
}

std::optional<AssetDescriptor> AssetDatabase::FindByGuid(const std::string& guid) const {
    std::shared_lock lock(m_cacheMutex);
    if (auto it = m_descriptorsByGuid.find(guid); it != m_descriptorsByGuid.end()) {
        return it->second;
    }
    return std::nullopt;
}

AssetDatabase::ListenerId AssetDatabase::RegisterListener(Listener listener) {
    if (!listener) {
        return 0;
    }
    std::unique_lock lock(m_listenerMutex);
    const ListenerId id = m_nextListenerId.fetch_add(1, std::memory_order_relaxed);
    m_listeners.emplace(id, std::move(listener));
    return id;
}

void AssetDatabase::UnregisterListener(ListenerId id) {
    if (id == 0) {
        return;
    }
    std::unique_lock lock(m_listenerMutex);
    m_listeners.erase(id);
}

void AssetDatabase::StartThreads() {
    std::lock_guard lock(m_stateMutex);
    if (m_indexThread.joinable()) {
        return;
    }

    try {
        m_indexThread = std::thread(&AssetDatabase::IndexThreadMain, this);
    } catch (const std::exception& ex) {
        gm::core::Logger::Error("[AssetDatabase] Failed to start index thread: {}", ex.what());
        m_stopRequested = true;
    }
}

void AssetDatabase::StopThreads() {
    {
        std::lock_guard lock(m_stateMutex);
        if (!m_indexThread.joinable()) {
            return;
        }
        m_stopRequested = true;
    }
    {
        std::lock_guard lock(m_conditionMutex);
        m_dirty = false;
    }
    m_condition.notify_all();
    m_indexThread.join();
}

void AssetDatabase::IndexThreadMain() {
    RebuildIndexes();

    while (true) {
        std::unique_lock lock(m_conditionMutex);
        m_condition.wait(lock, [this]() { return m_stopRequested || m_dirty; });
        if (m_stopRequested) {
            break;
        }
        m_dirty = false;
        lock.unlock();
        RebuildIndexes();
    }
}

void AssetDatabase::RebuildIndexes() {
    {
        std::lock_guard lock(m_conditionMutex);
        if (m_stopRequested) {
            return;
        }
        m_indexInProgress = true;
    }

    auto& catalog = AssetCatalog::Instance();
    auto assets = catalog.GetAllAssets();

    struct ShaderFilePair {
        std::optional<AssetDescriptor> vertex;
        std::optional<AssetDescriptor> fragment;
    };

    std::unordered_map<std::string, ShaderFilePair> shaderFilePairs;
    std::vector<MeshRecord> meshRecords;
    std::vector<PrefabRecord> prefabRecords;
    std::vector<ManifestRecord> manifestRecords;
    std::unordered_map<std::string, AssetDescriptor> descriptorsByGuid;

    shaderFilePairs.reserve(assets.size());
    meshRecords.reserve(assets.size());
    prefabRecords.reserve(assets.size());
    descriptorsByGuid.reserve(assets.size());

    for (const auto& asset : assets) {
        descriptorsByGuid[asset.guid] = asset;

        switch (asset.type) {
        case AssetType::Shader: {
            if (!IsUnderDirectory(asset.relativePath, "shaders/")) {
                break;
            }
            auto relativeLower = ToLower(asset.relativePath);
            auto baseKey = ShaderBaseKey(relativeLower);
            auto& pair = shaderFilePairs[baseKey];
            if (IsVertexShaderPath(relativeLower)) {
                pair.vertex = asset;
            } else if (IsFragmentShaderPath(relativeLower)) {
                pair.fragment = asset;
            }
            break;
        }
        case AssetType::Mesh:
            if (IsUnderDirectory(asset.relativePath, "models/")) {
                meshRecords.push_back({asset.guid, asset});
            }
            break;
        case AssetType::Prefab:
            if (IsUnderDirectory(asset.relativePath, "prefabs/")) {
                prefabRecords.push_back({asset.guid, asset});
            }
            break;
        default:
            if (asset.type == AssetType::Script) {
                if (asset.relativePath.find("manifest") != std::string::npos) {
                    manifestRecords.push_back({asset.guid, asset});
                }
            }
            break;
        }
    }

    std::vector<ShaderBatchRecord> shaderBatches;
    shaderBatches.reserve(shaderFilePairs.size());
    std::unordered_set<std::string> usedGuids;

    for (auto& [baseKey, files] : shaderFilePairs) {
        if (baseKey.empty() || !files.vertex || !files.fragment) {
            continue;
        }

        ShaderBatchRecord record;
        record.baseKey = baseKey;
        record.vertex = *files.vertex;
        record.fragment = *files.fragment;
        record.guid = GenerateDeterministicGuid("shader", baseKey);

        if (!usedGuids.insert(record.guid).second) {
            record.guid = GenerateDeterministicGuid("shader_alt", baseKey);
            usedGuids.insert(record.guid);
        }

        shaderBatches.push_back(std::move(record));
    }

    {
        std::unique_lock cacheLock(m_cacheMutex);
        m_shaderBatches = std::move(shaderBatches);
        m_meshRecords = std::move(meshRecords);
        m_prefabRecords = std::move(prefabRecords);
        m_manifestRecords = std::move(manifestRecords);
        m_descriptorsByGuid = std::move(descriptorsByGuid);
        m_indexVersion.fetch_add(1, std::memory_order_release);
    }

    {
        std::lock_guard lock(m_conditionMutex);
        m_indexInProgress = false;
    }
    m_ready.store(true, std::memory_order_release);
    m_condition.notify_all();
}

void AssetDatabase::HandleAssetEvent(const AssetEvent& event) {
    NotifyListeners(event);
    RequestRebuild();
}

void AssetDatabase::NotifyListeners(const AssetEvent& event) {
    std::shared_lock lock(m_listenerMutex);
    for (const auto& [id, listener] : m_listeners) {
        if (listener) {
            listener(event);
        }
    }
}

void AssetDatabase::RequestRebuild() {
    {
        std::lock_guard lock(m_conditionMutex);
        m_dirty = true;
    }
    m_condition.notify_all();
}

std::string AssetDatabase::ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool AssetDatabase::EndsWith(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() &&
           std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
}

bool AssetDatabase::IsVertexShaderPath(std::string_view relativeLower) {
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

bool AssetDatabase::IsFragmentShaderPath(std::string_view relativeLower) {
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

std::string AssetDatabase::ShaderBaseKey(std::string relativeLower) {
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

std::string AssetDatabase::GenerateDeterministicGuid(std::string_view prefix, std::string_view key) {
    auto hash = Fnv1a64(key);
    return fmt::format("{}::{:016x}", prefix, hash);
}

} // namespace gm::assets


