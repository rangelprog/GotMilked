#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <thread>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace gm::assets {

enum class AssetType {
    Unknown,
    Texture,
    Mesh,
    Shader,
    Material,
    Script,
    Audio,
    Prefab,
    Scene,
    Other
};

struct AssetDescriptor {
    std::string guid;
    AssetType type = AssetType::Unknown;
    std::string relativePath;
    std::filesystem::path absolutePath;
    std::filesystem::file_time_type lastWriteTime{};
};

enum class AssetEventType {
    Added,
    Updated,
    Removed
};

struct AssetEvent {
    AssetEventType type = AssetEventType::Added;
    AssetDescriptor descriptor;
};

class AssetCatalog {
public:
    using Listener = std::function<void(const AssetEvent&)>;
    using ListenerId = std::uint64_t;

    static AssetCatalog& Instance();

    void SetAssetRoot(std::filesystem::path root);
    [[nodiscard]] const std::filesystem::path& GetAssetRoot() const;

    void Scan();
    void StartWatching();
    void StopWatching();
    [[nodiscard]] bool IsWatching() const { return m_watchRunning.load(); }

    [[nodiscard]] std::vector<AssetDescriptor> GetAllAssets() const;
    [[nodiscard]] std::vector<AssetDescriptor> GetAssetsByType(AssetType type) const;
    [[nodiscard]] std::optional<AssetDescriptor> FindByGuid(const std::string& guid) const;
    [[nodiscard]] std::optional<AssetDescriptor> FindByRelativePath(const std::string& path) const;

    ListenerId RegisterListener(Listener listener);
    void UnregisterListener(ListenerId id);

private:
    AssetCatalog() = default;

    void NotifyListeners(const std::vector<AssetEvent>& events);
    static std::string ToCanonicalRelative(const std::filesystem::path& absolute, const std::filesystem::path& root);
    static AssetType Classify(const std::filesystem::path& path);
    static std::string GenerateGuid(const std::string& canonicalRelativePath);
    void WatchThreadMain();
    bool WatchWindows(const std::filesystem::path& root);
    void WatchPolling(const std::filesystem::path& root);

    std::filesystem::path m_assetRoot;
    std::unordered_map<std::string, AssetDescriptor> m_assetsByGuid;
    std::unordered_map<std::string, std::string> m_guidByRelativePath;

    mutable std::shared_mutex m_mutex;

    std::unordered_map<ListenerId, Listener> m_listeners;
    std::shared_mutex m_listenerMutex;
    std::atomic<ListenerId> m_nextListenerId{1};

    std::thread m_watchThread;
    std::atomic<bool> m_watchRunning{false};
    std::atomic<bool> m_watchStopRequested{false};
};

} // namespace gm::assets


