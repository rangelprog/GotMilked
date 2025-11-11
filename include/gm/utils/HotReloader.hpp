#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace gm::utils {

class HotReloader {
public:
    using ReloadCallback = std::function<bool()>;

    void SetEnabled(bool enabled) { m_enabled = enabled; }
    void SetPollInterval(double seconds) { m_pollIntervalSeconds = seconds; }
    bool IsEnabled() const { return m_enabled; }
    double GetPollInterval() const { return m_pollIntervalSeconds; }

    void AddWatch(const std::string& id,
                  const std::vector<std::filesystem::path>& paths,
                  ReloadCallback callback);

    void Update(double deltaSeconds);
    void ForcePoll();

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

    void Poll();

    bool m_enabled = true;
    double m_pollIntervalSeconds = 0.5;
    double m_accumulator = 0.0;
    std::vector<WatchEntry> m_watches;
};

} // namespace gm::utils

