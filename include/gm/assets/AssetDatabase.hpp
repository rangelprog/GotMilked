#pragma once

#include "gm/assets/AssetCatalog.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace gm::assets {

class AssetDatabase {
public:
    struct ShaderBatchRecord {
        std::string baseKey;
        std::string guid;
        AssetDescriptor vertex;
        AssetDescriptor fragment;
    };

    struct MeshRecord {
        std::string guid;
        AssetDescriptor descriptor;
    };

    struct PrefabRecord {
        std::string guid;
        AssetDescriptor descriptor;
    };

    struct ManifestRecord {
        std::string guid;
        AssetDescriptor descriptor;
    };

    using Listener = std::function<void(const AssetEvent&)>;
    using ListenerId = std::uint64_t;

    static AssetDatabase& Instance();

    void Initialize(const std::filesystem::path& assetRoot);
    void Shutdown();

    [[nodiscard]] bool IsInitialized() const { return m_initialized.load(std::memory_order_acquire); }
    [[nodiscard]] std::filesystem::path AssetRoot() const;

    void WaitForInitialIndex();
    void WaitUntilIdle();

    [[nodiscard]] std::uint64_t CurrentVersion() const { return m_indexVersion.load(std::memory_order_acquire); }

    [[nodiscard]] std::vector<ShaderBatchRecord> GetShaderBatches() const;
    [[nodiscard]] std::vector<MeshRecord> GetMeshRecords() const;
    [[nodiscard]] std::vector<PrefabRecord> GetPrefabRecords() const;
    [[nodiscard]] std::vector<ManifestRecord> GetManifestRecords() const;

    [[nodiscard]] std::optional<AssetDescriptor> FindByGuid(const std::string& guid) const;

    ListenerId RegisterListener(Listener listener);
    void UnregisterListener(ListenerId id);

private:
    AssetDatabase() = default;
    ~AssetDatabase();

    void StartThreads();
    void StopThreads();
    void IndexThreadMain();
    void RebuildIndexes();
    void HandleAssetEvent(const AssetEvent& event);
    void NotifyListeners(const AssetEvent& event);
    void RequestRebuild();

    static std::string ToLower(std::string value);
    static bool EndsWith(std::string_view value, std::string_view suffix);
    static bool IsVertexShaderPath(std::string_view relativeLower);
    static bool IsFragmentShaderPath(std::string_view relativeLower);
    static std::string ShaderBaseKey(std::string relativeLower);
    static std::string GenerateDeterministicGuid(std::string_view prefix, std::string_view key);

    std::filesystem::path m_assetRoot;

    mutable std::shared_mutex m_cacheMutex;
    std::vector<ShaderBatchRecord> m_shaderBatches;
    std::vector<MeshRecord> m_meshRecords;
    std::vector<PrefabRecord> m_prefabRecords;
    std::vector<ManifestRecord> m_manifestRecords;
    std::unordered_map<std::string, AssetDescriptor> m_descriptorsByGuid;

    std::thread m_indexThread;
    std::mutex m_stateMutex;
    std::mutex m_conditionMutex;
    std::condition_variable m_condition;
    std::atomic<bool> m_initialized{false};
    bool m_stopRequested = false;
    bool m_dirty = false;
    bool m_indexInProgress = false;
    std::atomic<bool> m_ready{false};
    std::atomic<std::uint64_t> m_indexVersion{0};

    mutable std::shared_mutex m_listenerMutex;
    std::unordered_map<ListenerId, Listener> m_listeners;
    std::atomic<ListenerId> m_nextListenerId{1};

    AssetCatalog::ListenerId m_catalogListener = 0;
};

} // namespace gm::assets


