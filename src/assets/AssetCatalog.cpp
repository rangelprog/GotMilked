#include "gm/assets/AssetCatalog.hpp"

#include "gm/core/Logger.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string_view>
#include <fmt/format.h>
#include <chrono>
#include <thread>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace gm::assets {

namespace {

std::string ToLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
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

} // namespace

AssetCatalog& AssetCatalog::Instance() {
    static AssetCatalog instance;
    return instance;
}

void AssetCatalog::SetAssetRoot(std::filesystem::path root) {
    StopWatching();
    std::unique_lock lock(m_mutex);
    if (root.empty()) {
        m_assetRoot.clear();
        m_assetsByGuid.clear();
        m_guidByRelativePath.clear();
        return;
    }

    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(root, ec);
    if (ec) {
        gm::core::Logger::Warning("[AssetCatalog] Failed to canonicalize asset root '{}': {}", root.string(), ec.message());
        canonical = std::move(root);
    }
    m_assetRoot = std::move(canonical);
}

const std::filesystem::path& AssetCatalog::GetAssetRoot() const {
    std::shared_lock lock(m_mutex);
    return m_assetRoot;
}

void AssetCatalog::Scan() {
    std::filesystem::path root;
    {
        std::shared_lock lock(m_mutex);
        root = m_assetRoot;
    }

    if (root.empty()) {
        gm::core::Logger::Warning("[AssetCatalog] Scan requested with empty asset root");
        return;
    }

    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) {
        gm::core::Logger::Warning("[AssetCatalog] Asset root '{}' does not exist", root.string());
        return;
    }

    std::unordered_map<std::string, AssetDescriptor> discovered;
    std::unordered_map<std::string, std::string> guidByRelative;

    const auto options = std::filesystem::directory_options::skip_permission_denied;
    for (std::filesystem::recursive_directory_iterator it(root, options), end; it != end; it.increment(ec)) {
        if (ec) {
            gm::core::Logger::Warning("[AssetCatalog] Directory iteration error: {}", ec.message());
            ec.clear();
            continue;
        }

        const auto& entry = *it;

        if (!entry.is_regular_file(ec)) {
            if (ec) {
                gm::core::Logger::Warning("[AssetCatalog] Error checking entry type: {}", ec.message());
                ec.clear();
            }
            continue;
        }

        auto relative = ToCanonicalRelative(entry.path(), root);
        if (relative.empty()) {
            continue;
        }

        auto guid = GenerateGuid(relative);

        AssetDescriptor descriptor;
        descriptor.guid = guid;
        descriptor.type = Classify(entry.path());
        descriptor.relativePath = relative;
        descriptor.absolutePath = entry.path();
        descriptor.lastWriteTime = entry.last_write_time(ec);
        if (ec) {
            ec.clear();
        }

        discovered[guid] = descriptor;
        guidByRelative[descriptor.relativePath] = guid;
    }

    std::vector<AssetEvent> events;
    {
        std::unique_lock lock(m_mutex);

        // Added or updated assets
        for (const auto& [guid, descriptor] : discovered) {
            const auto existing = m_assetsByGuid.find(guid);
            if (existing == m_assetsByGuid.end()) {
                events.push_back({AssetEventType::Added, descriptor});
            } else if (descriptor.lastWriteTime != existing->second.lastWriteTime ||
                       descriptor.absolutePath != existing->second.absolutePath) {
                events.push_back({AssetEventType::Updated, descriptor});
            }
        }

        // Removed assets
        for (const auto& [guid, descriptor] : m_assetsByGuid) {
            if (!discovered.contains(guid)) {
                events.push_back({AssetEventType::Removed, descriptor});
            }
        }

        m_assetsByGuid = std::move(discovered);
        m_guidByRelativePath = std::move(guidByRelative);
    }

    if (!events.empty()) {
        NotifyListeners(events);
    }
}

void AssetCatalog::StartWatching() {
    if (m_watchRunning.load()) {
        return;
    }

    const auto root = GetAssetRoot();
    if (root.empty()) {
        gm::core::Logger::Warning("[AssetCatalog] StartWatching requested with empty asset root");
        return;
    }

    m_watchStopRequested.store(false);
    try {
        m_watchThread = std::thread(&AssetCatalog::WatchThreadMain, this);
        m_watchRunning.store(true);
    } catch (const std::exception& ex) {
        m_watchRunning.store(false);
        gm::core::Logger::Error("[AssetCatalog] Failed to start watch thread: {}", ex.what());
    }
}

void AssetCatalog::StopWatching() {
    if (!m_watchRunning.load() && !m_watchThread.joinable()) {
        return;
    }

    m_watchStopRequested.store(true);
    if (m_watchThread.joinable()) {
        m_watchThread.join();
    }
    m_watchRunning.store(false);
    m_watchStopRequested.store(false);
}

std::vector<AssetDescriptor> AssetCatalog::GetAllAssets() const {
    std::shared_lock lock(m_mutex);
    std::vector<AssetDescriptor> results;
    results.reserve(m_assetsByGuid.size());
    for (const auto& [_, descriptor] : m_assetsByGuid) {
        results.push_back(descriptor);
    }
    std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
        return a.relativePath < b.relativePath;
    });
    return results;
}

std::vector<AssetDescriptor> AssetCatalog::GetAssetsByType(AssetType type) const {
    std::shared_lock lock(m_mutex);
    std::vector<AssetDescriptor> results;
    for (const auto& [_, descriptor] : m_assetsByGuid) {
        if (descriptor.type == type) {
            results.push_back(descriptor);
        }
    }
    std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
        return a.relativePath < b.relativePath;
    });
    return results;
}

std::optional<AssetDescriptor> AssetCatalog::FindByGuid(const std::string& guid) const {
    std::shared_lock lock(m_mutex);
    if (auto it = m_assetsByGuid.find(guid); it != m_assetsByGuid.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<AssetDescriptor> AssetCatalog::FindByRelativePath(const std::string& path) const {
    std::shared_lock lock(m_mutex);
    if (auto it = m_guidByRelativePath.find(ToLowerCopy(path)); it != m_guidByRelativePath.end()) {
        if (auto descriptorIt = m_assetsByGuid.find(it->second); descriptorIt != m_assetsByGuid.end()) {
            return descriptorIt->second;
        }
    }
    return std::nullopt;
}

AssetCatalog::ListenerId AssetCatalog::RegisterListener(Listener listener) {
    if (!listener) {
        return 0;
    }

    std::unique_lock lock(m_listenerMutex);
    const auto id = m_nextListenerId++;
    m_listeners.emplace(id, std::move(listener));
    return id;
}

void AssetCatalog::UnregisterListener(ListenerId id) {
    if (id == 0) {
        return;
    }
    std::unique_lock lock(m_listenerMutex);
    m_listeners.erase(id);
}

void AssetCatalog::NotifyListeners(const std::vector<AssetEvent>& events) {
    std::vector<Listener> listenersCopy;
    {
        std::shared_lock lock(m_listenerMutex);
        listenersCopy.reserve(m_listeners.size());
        for (const auto& [_, listener] : m_listeners) {
            listenersCopy.push_back(listener);
        }
    }

    for (const auto& listener : listenersCopy) {
        if (!listener) {
            continue;
        }
        for (const auto& event : events) {
            listener(event);
        }
    }
}

std::string AssetCatalog::ToCanonicalRelative(const std::filesystem::path& absolute, const std::filesystem::path& root) {
    std::error_code ec;
    auto relative = std::filesystem::relative(absolute, root, ec);
    if (ec) {
        gm::core::Logger::Warning("[AssetCatalog] Failed to build relative path for '{}': {}", absolute.string(), ec.message());
        return {};
    }
    auto canonical = relative.generic_string();
    return ToLowerCopy(canonical);
}

AssetType AssetCatalog::Classify(const std::filesystem::path& path) {
    const auto ext = ToLowerCopy(path.extension().string());
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp") {
        return AssetType::Texture;
    }
    if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb") {
        return AssetType::Mesh;
    }
    if (ext == ".vert" || ext == ".frag" || ext == ".glsl" || ext == ".vs" || ext == ".fs") {
        return AssetType::Shader;
    }
    if (ext == ".material" || ext == ".mat") {
        return AssetType::Material;
    }
    if (ext == ".json" || ext == ".yaml" || ext == ".yml") {
        const auto stem = ToLowerCopy(path.stem().string());
        if (stem.find("prefab") != std::string::npos) {
            return AssetType::Prefab;
        }
        if (stem.find("scene") != std::string::npos) {
            return AssetType::Scene;
        }
        return AssetType::Script;
    }
    if (ext == ".wav" || ext == ".ogg" || ext == ".mp3") {
        return AssetType::Audio;
    }
    return AssetType::Unknown;
}

std::string AssetCatalog::GenerateGuid(const std::string& canonicalRelativePath) {
    const auto hash = Fnv1a64(canonicalRelativePath);
    return fmt::format("{:016x}", hash);
}

void AssetCatalog::WatchThreadMain() {
    const auto root = GetAssetRoot();
    if (root.empty()) {
        m_watchRunning.store(false);
        return;
    }

#if defined(_WIN32)
    if (!WatchWindows(root)) {
        gm::core::Logger::Warning("[AssetCatalog] Falling back to polling watcher");
        WatchPolling(root);
    }
#else
    WatchPolling(root);
#endif

    m_watchRunning.store(false);
}

bool AssetCatalog::WatchWindows(const std::filesystem::path& root) {
#if defined(_WIN32)
    const auto rootWide = root.wstring();
    HANDLE changeHandle = FindFirstChangeNotificationW(
        rootWide.c_str(),
        TRUE,
        FILE_NOTIFY_CHANGE_FILE_NAME |
        FILE_NOTIFY_CHANGE_DIR_NAME |
        FILE_NOTIFY_CHANGE_LAST_WRITE |
        FILE_NOTIFY_CHANGE_SIZE);

    if (changeHandle == INVALID_HANDLE_VALUE) {
        gm::core::Logger::Warning("[AssetCatalog] FindFirstChangeNotification failed (error {})", GetLastError());
        return false;
    }

    gm::core::Logger::Info("[AssetCatalog] Started filesystem watcher for '{}'", root.string());

    constexpr auto kWaitTimeoutMs = 500u;
    const auto minInterval = std::chrono::milliseconds(200);
    auto lastScan = std::chrono::steady_clock::now();

    bool success = true;
    while (!m_watchStopRequested.load()) {
        const DWORD waitStatus = WaitForSingleObject(changeHandle, kWaitTimeoutMs);
        if (waitStatus == WAIT_OBJECT_0) {
            auto now = std::chrono::steady_clock::now();
            if (now - lastScan < minInterval) {
                std::this_thread::sleep_for(minInterval - (now - lastScan));
            }

            Scan();
            lastScan = std::chrono::steady_clock::now();

            if (!FindNextChangeNotification(changeHandle)) {
                gm::core::Logger::Warning("[AssetCatalog] FindNextChangeNotification failed (error {})", GetLastError());
                success = false;
                break;
            }
        } else if (waitStatus == WAIT_TIMEOUT) {
            continue;
        } else {
            gm::core::Logger::Warning("[AssetCatalog] WaitForSingleObject failed while watching assets (error {})", GetLastError());
            success = false;
            break;
        }
    }

    FindCloseChangeNotification(changeHandle);
    return success;
#else
    (void)root;
    return false;
#endif
}

void AssetCatalog::WatchPolling(const std::filesystem::path& root) {
    gm::core::Logger::Info("[AssetCatalog] Polling '{}' for asset changes", root.string());

    const auto interval = std::chrono::seconds(2);

    while (!m_watchStopRequested.load()) {
        std::this_thread::sleep_for(interval);
        if (m_watchStopRequested.load()) {
            break;
        }
        Scan();
    }
}

} // namespace gm::assets


