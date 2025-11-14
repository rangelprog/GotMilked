#include "gm/utils/HotReloader.hpp"
#include "gm/core/Logger.hpp"

#include <algorithm>
#include <system_error>
#include <thread>
#include <atomic>
#include <mutex>
#include <future>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <sys/inotify.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <unordered_map>
#endif

namespace gm::utils {

// Platform-specific file watcher implementation
// Note: This is defined here (not in header) because it's an implementation detail
class HotReloader::FileWatcherImpl {
public:
    FileWatcherImpl() = default;
    virtual ~FileWatcherImpl() = default;

    virtual bool StartWatching(const std::vector<std::filesystem::path>& paths,
                               std::function<void(const std::filesystem::path&)> onChange) = 0;
    virtual void StopWatching() = 0;
    virtual void Update() = 0;  // Process pending events
    virtual bool IsSupported() const = 0;
};

#ifdef _WIN32
// Windows implementation using ReadDirectoryChangesW
class WindowsFileWatcher : public HotReloader::FileWatcherImpl {
public:
    WindowsFileWatcher() : m_directoryHandle(INVALID_HANDLE_VALUE), m_stopRequested(false) {}
    
    ~WindowsFileWatcher() override {
        StopWatching();
    }

    bool StartWatching(const std::vector<std::filesystem::path>& paths,
                      std::function<void(const std::filesystem::path&)> onChange) override {
        if (paths.empty()) {
            return false;
        }

        // Stop any existing watching first
        if (m_directoryHandle != INVALID_HANDLE_VALUE) {
            StopWatching();
        }

        std::filesystem::path watchDir = paths[0].parent_path();
        if (watchDir.empty()) {
            watchDir = std::filesystem::current_path();
        }

        // Validate directory exists
        std::error_code ec;
        if (!std::filesystem::exists(watchDir, ec) || !std::filesystem::is_directory(watchDir, ec)) {
            gm::core::Logger::Warning("[HotReloader] Watch directory does not exist: {}", watchDir.string());
            return false;
        }

        m_watchedPaths = paths;
        m_onChange = onChange;

        try {
            std::wstring dirPath = watchDir.wstring();
            if (dirPath.empty()) {
                gm::core::Logger::Warning("[HotReloader] Failed to convert watch directory to wide string");
                return false;
            }

            m_directoryHandle = CreateFileW(
                dirPath.c_str(),
                FILE_LIST_DIRECTORY,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                nullptr
            );

            if (m_directoryHandle == INVALID_HANDLE_VALUE) {
                DWORD error = GetLastError();
                gm::core::Logger::Warning("[HotReloader] Failed to open directory for watching: {} (error: {})", 
                                        watchDir.string(), error);
                return false;
            }

            m_stopRequested = false;
            try {
                m_watchThread = std::thread([this, watchDir]() {
                    WatchThread(watchDir);
                });
            } catch (const std::exception& e) {
                CloseHandle(m_directoryHandle);
                m_directoryHandle = INVALID_HANDLE_VALUE;
                gm::core::Logger::Error("[HotReloader] Failed to create watch thread: {}", e.what());
                return false;
            }
        } catch (const std::exception& e) {
            gm::core::Logger::Error("[HotReloader] Exception in StartWatching: {}", e.what());
            return false;
        }

        return true;
    }

    void StopWatching() override {
        if (m_directoryHandle == INVALID_HANDLE_VALUE) {
            return;
        }

        m_stopRequested = true;
        CancelIo(m_directoryHandle);
        
        if (m_watchThread.joinable()) {
            m_watchThread.join();
        }

        CloseHandle(m_directoryHandle);
        m_directoryHandle = INVALID_HANDLE_VALUE;
    }

    void Update() override {
        // Events are processed in the watch thread
    }

    bool IsSupported() const override {
        return true;
    }

private:
    void WatchThread(const std::filesystem::path& watchDir) {
        constexpr DWORD bufferSize = 4096;
        char buffer[bufferSize];
        DWORD bytesReturned;
        OVERLAPPED overlapped = {};
        overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

        if (overlapped.hEvent == nullptr) {
            gm::core::Logger::Error("[HotReloader] Failed to create event for watch thread");
            return;
        }

        while (!m_stopRequested) {
            ResetEvent(overlapped.hEvent);

            // Clear the buffer
            ZeroMemory(buffer, bufferSize);
            bytesReturned = 0;

            BOOL success = ReadDirectoryChangesW(
                m_directoryHandle,
                buffer,
                bufferSize,
                FALSE,
                FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
                &bytesReturned,
                &overlapped,
                nullptr);

            if (!success) {
                DWORD error = GetLastError();
                if (error != ERROR_IO_PENDING) {
                    if (error != ERROR_INVALID_HANDLE) {
                        gm::core::Logger::Warning("[HotReloader] ReadDirectoryChangesW failed: {}", error);
                    }
                    break;
                }
            }

            DWORD waitResult = WaitForSingleObject(overlapped.hEvent, 100);
            if (waitResult == WAIT_OBJECT_0) {
                if (GetOverlappedResult(m_directoryHandle, &overlapped, &bytesReturned, FALSE)) {
                    if (bytesReturned > 0) {
                        ProcessChanges(buffer, bytesReturned, watchDir);
                    }
                } else {
                    DWORD error = GetLastError();
                    if (error != ERROR_IO_INCOMPLETE && error != ERROR_INVALID_HANDLE) {
                        gm::core::Logger::Warning("[HotReloader] GetOverlappedResult failed: {}", error);
                    }
                }
            } else if (waitResult == WAIT_FAILED) {
                DWORD error = GetLastError();
                gm::core::Logger::Warning("[HotReloader] WaitForSingleObject failed: {}", error);
                break;
            }
        }

        if (overlapped.hEvent != nullptr) {
            CloseHandle(overlapped.hEvent);
        }
    }

    void ProcessChanges(char* buffer, DWORD bytesReturned, const std::filesystem::path& watchDir) {
        if (bytesReturned == 0 || buffer == nullptr) {
            return;
        }

        DWORD offset = 0;
        while (offset < bytesReturned) {
            if (offset + sizeof(FILE_NOTIFY_INFORMATION) > bytesReturned) {
                break;  // Prevent buffer overrun
            }

            FILE_NOTIFY_INFORMATION* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer + offset);
            
            if (info->FileNameLength == 0) {
                if (info->NextEntryOffset == 0) {
                    break;
                }
                offset += info->NextEntryOffset;
                continue;
            }

            // Validate filename length
            if (info->FileNameLength > MAX_PATH * sizeof(WCHAR)) {
                if (info->NextEntryOffset == 0) {
                    break;
                }
                offset += info->NextEntryOffset;
                continue;
            }
            
            try {
                std::wstring fileNameW(info->FileName, info->FileNameLength / sizeof(WCHAR));
                std::filesystem::path changedFile = watchDir / fileNameW;

                for (const auto& watchedPath : m_watchedPaths) {
                    try {
                        if (std::filesystem::equivalent(changedFile, watchedPath) ||
                            changedFile == watchedPath) {
                            // Queue the callback for execution on main thread
                            if (m_onChange) {
                                try {
                                    m_onChange(watchedPath);
                                } catch (const std::exception& e) {
                                    // Log but don't crash if callback throws
                                    gm::core::Logger::Error("[HotReloader] Callback exception: {}", e.what());
                                } catch (...) {
                                    gm::core::Logger::Error("[HotReloader] Callback threw unknown exception");
                                }
                            }
                            break;
                        }
                    } catch (const std::filesystem::filesystem_error&) {
                        // Continue on error
                    }
                }
            } catch (const std::exception& e) {
                gm::core::Logger::Warning("[HotReloader] Exception processing file change: {}", e.what());
            }

            if (info->NextEntryOffset == 0) {
                break;
            }
            offset += info->NextEntryOffset;
        }
    }

    HANDLE m_directoryHandle;
    std::atomic<bool> m_stopRequested;
    std::thread m_watchThread;
    std::vector<std::filesystem::path> m_watchedPaths;
    std::function<void(const std::filesystem::path&)> m_onChange;
};

#elif defined(__linux__)
// Linux implementation using inotify
class LinuxFileWatcher : public HotReloader::FileWatcherImpl {
public:
    LinuxFileWatcher() : m_inotifyFd(-1) {}
    
    ~LinuxFileWatcher() override {
        StopWatching();
    }

    bool StartWatching(const std::vector<std::filesystem::path>& paths,
                      std::function<void(const std::filesystem::path&)> onChange) override {
        m_inotifyFd = inotify_init1(IN_NONBLOCK);
        if (m_inotifyFd < 0) {
            return false;
        }

        m_watchedPaths = paths;
        m_onChange = onChange;

        for (const auto& path : paths) {
            std::filesystem::path parent = path.parent_path();
            if (parent.empty()) {
                parent = std::filesystem::current_path();
            }

            int wd = inotify_add_watch(m_inotifyFd, parent.c_str(), 
                                      IN_MODIFY | IN_CLOSE_WRITE | IN_MOVED_TO);
            if (wd >= 0) {
                m_watchDescriptors[wd] = path;
            }
        }

        return true;
    }

    void StopWatching() override {
        if (m_inotifyFd >= 0) {
            close(m_inotifyFd);
            m_inotifyFd = -1;
        }
        m_watchDescriptors.clear();
    }

    void Update() override {
        if (m_inotifyFd < 0) {
            return;
        }

        constexpr size_t bufferSize = 4096;
        char buffer[bufferSize];
        ssize_t length = read(m_inotifyFd, buffer, bufferSize);

        if (length < 0) {
            return;
        }

        size_t i = 0;
        while (i < static_cast<size_t>(length)) {
            inotify_event* event = reinterpret_cast<inotify_event*>(&buffer[i]);
            if (event->len > 0) {
                auto it = m_watchDescriptors.find(event->wd);
                if (it != m_watchDescriptors.end()) {
                    std::filesystem::path changedFile = it->second.parent_path() / event->name;
                    try {
                        if (std::filesystem::equivalent(changedFile, it->second) || changedFile == it->second) {
                            if (m_onChange) {
                                m_onChange(it->second);
                            }
                        }
                    } catch (const std::filesystem::filesystem_error&) {
                        // Continue on error
                    }
                }
            }
            i += sizeof(inotify_event) + event->len;
        }
    }

    bool IsSupported() const override {
        return true;
    }

private:
    int m_inotifyFd;
    std::vector<std::filesystem::path> m_watchedPaths;
    std::unordered_map<int, std::filesystem::path> m_watchDescriptors;
    std::function<void(const std::filesystem::path&)> m_onChange;
};

#else
// Fallback: no native file watching support
class FallbackFileWatcher : public HotReloader::FileWatcherImpl {
public:
    bool StartWatching(const std::vector<std::filesystem::path>& paths,
                      std::function<void(const std::filesystem::path&)> onChange) override {
        return false;
    }

    void StopWatching() override {}
    void Update() override {}
    bool IsSupported() const override { return false; }
};
#endif

namespace {

std::filesystem::file_time_type GetTimestamp(const std::filesystem::path& path,
                                             bool& ok) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        ok = false;
        return {};
    }
    auto ts = std::filesystem::last_write_time(path, ec);
    ok = !ec;
    return ts;
}

} // namespace

HotReloader::HotReloader() {
    try {
#ifdef _WIN32
        m_watcherImpl = std::make_unique<WindowsFileWatcher>();
#elif defined(__linux__)
        m_watcherImpl = std::make_unique<LinuxFileWatcher>();
#else
        m_watcherImpl = std::make_unique<FallbackFileWatcher>();
#endif
    } catch (const std::exception& e) {
        gm::core::Logger::Error("[HotReloader] Failed to create file watcher: {}", e.what());
#ifdef _WIN32
        // On Windows, try to create a WindowsFileWatcher again, or leave as nullptr
        // The polling fallback will work even without a watcher
        m_watcherImpl = nullptr;
#elif defined(__linux__)
        m_watcherImpl = nullptr;
#else
        m_watcherImpl = std::make_unique<FallbackFileWatcher>();
#endif
    } catch (...) {
        gm::core::Logger::Error("[HotReloader] Failed to create file watcher: unknown exception");
#ifdef _WIN32
        m_watcherImpl = nullptr;
#elif defined(__linux__)
        m_watcherImpl = nullptr;
#else
        m_watcherImpl = std::make_unique<FallbackFileWatcher>();
#endif
    }
}

HotReloader::~HotReloader() {
    if (m_watcherImpl) {
        m_watcherImpl->StopWatching();
    }
}

void HotReloader::SetEnabled(bool enabled) {
    m_enabled = enabled;
    if (!enabled && m_watcherImpl) {
        m_watcherImpl->StopWatching();
    }
}

void HotReloader::AddWatch(const std::string& id,
                           const std::vector<std::filesystem::path>& paths,
                           ReloadCallback callback) {
    if (paths.empty() || !callback) {
        gm::core::Logger::Warning("[HotReloader] Ignoring watch '{}' with no paths or callback",
                                  id);
        return;
    }

    std::lock_guard<std::mutex> lock(m_watchesMutex);

    WatchEntry entry;
    entry.id = id;
    entry.callback = std::move(callback);
    entry.paths.reserve(paths.size());

    for (const auto& path : paths) {
        WatchedPath watched;
        watched.path = path;
        bool ok = false;
        watched.timestamp = GetTimestamp(path, ok);
        watched.missingLogged = !ok;
        if (!ok) {
            gm::core::Logger::Warning("[HotReloader] File '{}' for watch '{}' missing or inaccessible",
                                      path.string(), id);
        }
        entry.paths.emplace_back(std::move(watched));
    }

    m_watches.emplace_back(std::move(entry));

    // Try to use native file watching if supported
    // Native file watchers call callbacks from background threads, so we queue them
    // for execution on the main thread in Update()
    // NOTE: Currently disabled because WindowsFileWatcher only supports one directory at a time,
    // and multiple AddWatch() calls would interfere with each other. Use polling for now.
    // TODO: Implement multi-directory support or aggregate watches by directory
    bool useNativeWatching = false;  // Temporarily disabled until multi-watch support is implemented
    
    if (useNativeWatching && m_enabled && m_watcherImpl && m_watcherImpl->IsSupported()) {
        // Set up file watcher callback
        // This callback runs in a background thread, so we queue the callback
        // for execution on the main thread instead of calling it directly
        auto onChange = [this, id](const std::filesystem::path& changedPath) {
            // Queue the callback for execution on the main thread
            try {
                std::lock_guard<std::mutex> lock(m_pendingCallbacksMutex);
                m_pendingCallbacks.push_back({id, changedPath});
                // Don't log here as it might be called frequently
            } catch (const std::exception& e) {
                gm::core::Logger::Error("[HotReloader] Exception queuing callback: {}", e.what());
            } catch (...) {
                gm::core::Logger::Error("[HotReloader] Unknown exception queuing callback");
            }
        };

        try {
            if (m_watcherImpl && m_watcherImpl->StartWatching(paths, onChange)) {
                gm::core::Logger::Info("[HotReloader] Using native file watcher for '{}'", id);
                return;  // Native watching is active, no need for polling
            }
        } catch (const std::exception& e) {
            gm::core::Logger::Error("[HotReloader] Exception starting native watcher for '{}': {}", id, e.what());
        } catch (...) {
            gm::core::Logger::Error("[HotReloader] Unknown exception starting native watcher for '{}'", id);
        }
    }

    // Fall back to polling if native watching is not available or failed
    // Polling is safe and works for multiple watches
}

void HotReloader::Update(double deltaSeconds) {
    if (!m_enabled) {
        return;
    }

    // Process pending callbacks from native file watchers (executes on main thread)
    ProcessPendingCallbacks();

    // Update native file watcher if available
    if (m_watcherImpl && m_watcherImpl->IsSupported()) {
        m_watcherImpl->Update();
    }

    // Fall back to polling for watches that don't have native watching
    // or if native watching is not supported
    std::lock_guard<std::mutex> lock(m_watchesMutex);
    if (!m_watches.empty()) {
        m_accumulator += deltaSeconds;
        if (m_accumulator >= m_pollIntervalSeconds) {
            m_accumulator = 0.0;
            Poll();
        }
    }
}

void HotReloader::ForcePoll() {
    if (!m_enabled) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_watchesMutex);
    if (m_watches.empty()) {
        return;
    }
    Poll();
}

void HotReloader::ProcessPendingCallbacks() {
    // Move pending callbacks to local vector to minimize lock time
    std::vector<PendingCallback> callbacksToProcess;
    {
        try {
            std::lock_guard<std::mutex> lock(m_pendingCallbacksMutex);
            if (m_pendingCallbacks.empty()) {
                return;
            }
            callbacksToProcess = std::move(m_pendingCallbacks);
            m_pendingCallbacks.clear();
        } catch (const std::exception& e) {
            gm::core::Logger::Error("[HotReloader] Exception accessing pending callbacks: {}", e.what());
            return;
        }
    }

    // Now process callbacks on the main thread (m_watchesMutex is not locked here)
    std::lock_guard<std::mutex> lock(m_watchesMutex);
    for (const auto& pending : callbacksToProcess) {
        for (auto& watch : m_watches) {
            if (watch.id == pending.watchId) {
                // Check if the changed path matches any of our watched paths
                for (auto& watchedPath : watch.paths) {
                    try {
                        if (std::filesystem::equivalent(pending.changedPath, watchedPath.path) ||
                            pending.changedPath == watchedPath.path) {
                            // Execute callback on main thread (thread-safe now)
                            bool success = false;
                            if (watch.callback) {
                                try {
                                    success = watch.callback();
                                } catch (const std::exception& e) {
                                    gm::core::Logger::Error("[HotReloader] Callback exception for '{}': {}", 
                                                           pending.watchId, e.what());
                                } catch (...) {
                                    gm::core::Logger::Error("[HotReloader] Callback threw unknown exception for '{}'", 
                                                           pending.watchId);
                                }
                            }
                            
                            if (success) {
                                // Update timestamp
                                bool ok = false;
                                watchedPath.timestamp = GetTimestamp(watchedPath.path, ok);
                                gm::core::Logger::Info("[HotReloader] Reloaded '{}' successfully", pending.watchId);
                            } else {
                                gm::core::Logger::Warning("[HotReloader] Reload failed for '{}'", pending.watchId);
                            }
                            break;  // Found matching path, move to next callback
                        }
                    } catch (const std::filesystem::filesystem_error&) {
                        // Filesystem error comparing paths, continue
                    }
                }
                break;  // Found matching watch, move to next callback
            }
        }
    }
}

void HotReloader::Poll() {
    // Note: This method should be called with m_watchesMutex already locked
    for (auto& watch : m_watches) {
        bool shouldReload = false;
        std::vector<std::filesystem::file_time_type> newTimestamps;
        newTimestamps.reserve(watch.paths.size());

        for (auto& pathEntry : watch.paths) {
            bool ok = false;
            auto timestamp = GetTimestamp(pathEntry.path, ok);

            if (!ok) {
                if (!pathEntry.missingLogged) {
                    gm::core::Logger::Warning("[HotReloader] File '{}' for watch '{}' missing or unreadable",
                                              pathEntry.path.string(), watch.id);
                    pathEntry.missingLogged = true;
                }
                newTimestamps.emplace_back(pathEntry.timestamp);
                continue;
            }

            if (pathEntry.missingLogged) {
                gm::core::Logger::Info("[HotReloader] File '{}' for watch '{}' is now available",
                                       pathEntry.path.string(), watch.id);
                pathEntry.missingLogged = false;
            }

            newTimestamps.emplace_back(timestamp);
            if (pathEntry.timestamp != timestamp) {
                shouldReload = true;
            }
        }

        if (shouldReload) {
            gm::core::Logger::Info("[HotReloader] Detected change for '{}', reloading...", watch.id);
            bool success = watch.callback ? watch.callback() : false;
            if (success) {
                for (std::size_t i = 0; i < watch.paths.size(); ++i) {
                    watch.paths[i].timestamp = newTimestamps[i];
                }
                gm::core::Logger::Info("[HotReloader] Reloaded '{}' successfully", watch.id);
            } else {
                gm::core::Logger::Warning("[HotReloader] Reload failed for '{}'", watch.id);
            }
        }
    }
}

} // namespace gm::utils

