#include "ResourceHotReloader.hpp"

#include <system_error>
#include <utility>

#include "gm/core/Logger.hpp"

namespace sandbox {

namespace {

static std::filesystem::file_time_type GetTimestamp(const std::filesystem::path& path,
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

void ResourceHotReloader::AddWatch(const std::string& id,
                                   const std::vector<std::filesystem::path>& paths,
                                   ReloadCallback callback) {
    if (paths.empty() || !callback) {
        gm::core::Logger::Warning("[ResourceHotReloader] Ignoring watch '%s' with no paths or callback",
                                  id.c_str());
        return;
    }

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
            gm::core::Logger::Warning("[ResourceHotReloader] File '%s' for watch '%s' missing or inaccessible",
                                      path.string().c_str(), id.c_str());
        }
        entry.paths.emplace_back(std::move(watched));
    }

    m_watches.emplace_back(std::move(entry));
}

void ResourceHotReloader::Update(double deltaSeconds) {
    if (!m_enabled || m_watches.empty()) {
        return;
    }

    m_accumulator += deltaSeconds;
    if (m_accumulator >= m_pollIntervalSeconds) {
        m_accumulator = 0.0;
        Poll();
    }
}

void ResourceHotReloader::ForcePoll() {
    if (!m_enabled || m_watches.empty()) {
        return;
    }
    Poll();
}

void ResourceHotReloader::Poll() {
    for (auto& watch : m_watches) {
        bool shouldReload = false;
        std::vector<std::filesystem::file_time_type> newTimestamps;
        newTimestamps.reserve(watch.paths.size());

        for (auto& pathEntry : watch.paths) {
            bool ok = false;
            auto timestamp = GetTimestamp(pathEntry.path, ok);

            if (!ok) {
                if (!pathEntry.missingLogged) {
                    gm::core::Logger::Warning("[ResourceHotReloader] File '%s' for watch '%s' missing or unreadable",
                                              pathEntry.path.string().c_str(), watch.id.c_str());
                    pathEntry.missingLogged = true;
                }
                newTimestamps.emplace_back(pathEntry.timestamp);
                continue;
            }

            if (pathEntry.missingLogged) {
                gm::core::Logger::Info("[ResourceHotReloader] File '%s' for watch '%s' is now available",
                                       pathEntry.path.string().c_str(), watch.id.c_str());
                pathEntry.missingLogged = false;
            }

            newTimestamps.emplace_back(timestamp);
            if (pathEntry.timestamp != timestamp) {
                shouldReload = true;
            }
        }

        if (shouldReload) {
            gm::core::Logger::Info("[ResourceHotReloader] Detected change for '%s', reloading...", watch.id.c_str());
            bool success = watch.callback ? watch.callback() : false;
            if (success) {
                for (std::size_t i = 0; i < watch.paths.size(); ++i) {
                    watch.paths[i].timestamp = newTimestamps[i];
                }
                gm::core::Logger::Info("[ResourceHotReloader] Reloaded '%s' successfully", watch.id.c_str());
            } else {
                gm::core::Logger::Warning("[ResourceHotReloader] Reload failed for '%s'", watch.id.c_str());
            }
        }
    }
}

} // namespace sandbox

