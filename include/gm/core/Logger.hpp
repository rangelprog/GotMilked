#pragma once
#include <atomic>
#include <cstdlib>
#include <string>
#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>
#include <functional>
#include <chrono>
#include <system_error>

#include <fmt/format.h>
#include <fmt/compile.h>
#include <fmt/std.h>

namespace gm {
namespace core {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class Logger {
public:
    using LogCallback = std::function<void(LogLevel, const std::string&)>;

    template<typename... Args>
    static void Debug(fmt::format_string<Args...> format, Args&&... args) {
#ifdef GM_DEBUG
        if (!ShouldLogDebug()) {
            return;
        }
        LogFormatted(LogLevel::Debug, format, std::forward<Args>(args)...);
#else
        (void)format;
        (void)sizeof...(Args);
        (void)std::initializer_list<int>{((void)args, 0)...};
#endif
    }

    template<typename... Args>
    static void Debug(fmt::runtime_format_string<Args...> format, Args&&... args) {
#ifdef GM_DEBUG
        if (!ShouldLogDebug()) {
            return;
        }
        LogRuntime(LogLevel::Debug, format, std::forward<Args>(args)...);
#else
        (void)format;
        (void)sizeof...(Args);
        (void)std::initializer_list<int>{((void)args, 0)...};
#endif
    }

    template<typename... Args>
    static void Info(fmt::format_string<Args...> format, Args&&... args) {
        LogFormatted(LogLevel::Info, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Info(fmt::runtime_format_string<Args...> format, Args&&... args) {
        LogRuntime(LogLevel::Info, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Warning(fmt::format_string<Args...> format, Args&&... args) {
        LogFormatted(LogLevel::Warning, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Warning(fmt::runtime_format_string<Args...> format, Args&&... args) {
        LogRuntime(LogLevel::Warning, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Error(fmt::format_string<Args...> format, Args&&... args) {
        LogFormatted(LogLevel::Error, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Error(fmt::runtime_format_string<Args...> format, Args&&... args) {
        LogRuntime(LogLevel::Error, format, std::forward<Args>(args)...);
    }

    static void SetDebugEnabled(bool enabled) {
#ifdef GM_DEBUG
        s_debugEnabled.store(enabled, std::memory_order_release);
        s_configured.store(true, std::memory_order_release);
#else
        (void)enabled;
#endif
    }

    static bool IsDebugEnabled() {
#ifdef GM_DEBUG
        EnsureConfigured();
        return s_debugEnabled.load(std::memory_order_acquire);
#else
        return false;
#endif
    }

    static void ConfigureFromEnvironment() {
#ifdef GM_DEBUG
        ConfigureFromEnvironmentInternal();
        s_configured.store(true, std::memory_order_release);
#endif
    }

    static void SetLogFile(const std::filesystem::path& path) {
        std::lock_guard<std::mutex> lock(s_logMutex);
        s_logFilePath = path;
        if (s_logStream.is_open()) {
            s_logStream.close();
        }
        s_logStreamOpen = false;
        if (!s_logFilePath.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(s_logFilePath.parent_path(), ec);
            s_logStream.open(s_logFilePath, std::ios::out | std::ios::app);
            s_logStreamOpen = s_logStream.is_open();
        }
    }

    static size_t RegisterListener(LogCallback callback) {
        if (!callback) {
            return 0;
        }
        std::lock_guard<std::mutex> lock(s_logMutex);
        const size_t token = s_listenerCounter.fetch_add(1, std::memory_order_relaxed);
        s_listeners.emplace_back(token, std::move(callback));
        return token;
    }

    static void UnregisterListener(size_t token) {
        if (token == 0) {
            return;
        }
        std::lock_guard<std::mutex> lock(s_logMutex);
        auto it = std::remove_if(s_listeners.begin(), s_listeners.end(),
                                 [token](const auto& entry) { return entry.first == token; });
        s_listeners.erase(it, s_listeners.end());
    }

private:
    template<typename... Args>
    static void LogFormatted(LogLevel level, fmt::format_string<Args...> format, Args&&... args) {
        Write(level, fmt::format(format, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void LogRuntime(LogLevel level, fmt::runtime_format_string<Args...> format, Args&&... args) {
        Write(level, fmt::format(format, std::forward<Args>(args)...));
    }

    static void Write(LogLevel level, const std::string& message) {
        const std::string timestamp = FormatTimestamp();
        const std::string line = fmt::format("[{}] {}{}", timestamp, LogLevelPrefix(level), message);

        fmt::print(stderr, "{}\n", line);

        std::vector<LogCallback> listenersCopy;
        {
            std::lock_guard<std::mutex> lock(s_logMutex);
            EnsureLogStreamLocked();
            if (s_logStreamOpen && s_logStream.is_open()) {
                s_logStream << line << '\n';
                s_logStream.flush();
            }
            listenersCopy.reserve(s_listeners.size());
            for (const auto& [token, callback] : s_listeners) {
                if (callback) {
                    listenersCopy.push_back(callback);
                }
            }
        }

        for (auto& callback : listenersCopy) {
            callback(level, line);
        }
    }

    static constexpr const char* LogLevelPrefix(LogLevel level) {
        switch (level) {
            case LogLevel::Debug:   return "[Debug] ";
            case LogLevel::Info:    return "[Info] ";
            case LogLevel::Warning: return "[Warning] ";
            case LogLevel::Error:   return "[Error] ";
        }
        return "";
    }

    static std::string FormatTimestamp() {
        using namespace std::chrono;
        const auto now = system_clock::now();
        const auto timeT = system_clock::to_time_t(now);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &timeT);
#else
        localtime_r(&timeT, &tm);
#endif
        const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
        return fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}",
                           tm.tm_year + 1900,
                           tm.tm_mon + 1,
                           tm.tm_mday,
                           tm.tm_hour,
                           tm.tm_min,
                           tm.tm_sec,
                           ms.count());
    }

    static void EnsureLogStreamLocked() {
        if (s_logStreamOpen || s_logFilePath.empty()) {
            return;
        }
        std::error_code ec;
        std::filesystem::create_directories(s_logFilePath.parent_path(), ec);
        s_logStream.open(s_logFilePath, std::ios::out | std::ios::app);
        s_logStreamOpen = s_logStream.is_open();
    }

    #ifdef GM_DEBUG
    static inline std::atomic<bool> s_debugEnabled{true};
    static inline std::atomic<bool> s_configured{false};

    static bool ShouldLogDebug() {
        EnsureConfigured();
        return s_debugEnabled.load(std::memory_order_relaxed);
    }

    static void EnsureConfigured() {
        if (!s_configured.load(std::memory_order_acquire)) {
            bool expected = false;
            if (s_configured.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
                ConfigureFromEnvironmentInternal();
            }
        }
    }

    static void ConfigureFromEnvironmentInternal() {
        const char* env = std::getenv("GM_LOG_DEBUG");
        if (!env) {
            return;
        }
        std::string value(env);
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (value == "1" || value == "true" || value == "on" || value == "yes") {
            s_debugEnabled.store(true, std::memory_order_release);
        } else if (value == "0" || value == "false" || value == "off" || value == "no") {
            s_debugEnabled.store(false, std::memory_order_release);
        }
    }
    #else
    static void EnsureConfigured() {}
    static void ConfigureFromEnvironmentInternal() {}
    #endif

    static inline std::mutex s_logMutex{};
    static inline std::filesystem::path s_logFilePath{};
    static inline std::ofstream s_logStream{};
    static inline bool s_logStreamOpen = false;
    static inline std::vector<std::pair<size_t, LogCallback>> s_listeners{};
    static inline std::atomic<size_t> s_listenerCounter{1};
};

}} // namespace gm::core