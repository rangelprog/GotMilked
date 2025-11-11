#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>

namespace gm::utils {

class HotReloader {
public:
    using ReloadCallback = std::function<bool()>;

    HotReloader();
    ~HotReloader();

    void SetEnabled(bool enabled);
    void SetPollInterval(double seconds) { m_pollIntervalSeconds = seconds; }
    bool IsEnabled() const { return m_enabled; }
    double GetPollInterval() const { return m_pollIntervalSeconds; }

    void AddWatch(const std::string& id,
                  const std::vector<std::filesystem::path>& paths,
                  ReloadCallback callback);

    void Update(double deltaSeconds);
    void ForcePoll();

    // Platform-specific file watcher implementation (forward declaration)
    // Public so derived classes in implementation file can inherit from it
    class FileWatcherImpl;

private:
    struct WatchedPath {
        std::filesystem::path path;
        std::filesystem::file_time_type timestamp{};
        bool missingLogged = false;
    };

    struct WatchEntry {
        std::string id;
        std::vector<WatchedPath> paths;
        ReloadCallback callback;
    };

    std::unique_ptr<FileWatcherImpl> m_watcherImpl;

    void Poll();  // Fallback polling method

    // Deferred callback execution for thread-safe native file watching
    struct PendingCallback {
        std::string watchId;
        std::filesystem::path changedPath;
    };
    std::vector<PendingCallback> m_pendingCallbacks;
    std::mutex m_pendingCallbacksMutex;  // Protect callback queue from concurrent access

    void ProcessPendingCallbacks();  // Execute queued callbacks on main thread

    bool m_enabled = true;
    double m_pollIntervalSeconds = 0.5;
    double m_accumulator = 0.0;
    std::vector<WatchEntry> m_watches;
    std::mutex m_watchesMutex;  // Protect m_watches from concurrent access
};

} // namespace gm::utils

