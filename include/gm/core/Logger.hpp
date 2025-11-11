#pragma once
#include <cstdio>
#include <string>
#include <cstdarg>
#include <atomic>
#include <cstdlib>
#include <algorithm>
#include <cctype>

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
    static void Debug(const char* fmt, ...) {
        #ifdef GM_DEBUG
        EnsureConfigured();
        if (!s_debugEnabled.load(std::memory_order_relaxed)) {
            return;
        }
        va_list args;
        va_start(args, fmt);
        Log(LogLevel::Debug, fmt, args);
        va_end(args);
        #endif
    }

    static void Info(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        Log(LogLevel::Info, fmt, args);
        va_end(args);
    }

    static void Warning(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        Log(LogLevel::Warning, fmt, args);
        va_end(args);
    }

    static void Error(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        Log(LogLevel::Error, fmt, args);
        va_end(args);
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

private:
    static void Log(LogLevel level, const char* fmt, va_list args) {
        const char* prefix = "";
        switch (level) {
            case LogLevel::Debug:   prefix = "[Debug] "; break;
            case LogLevel::Info:    prefix = "[Info] "; break;
            case LogLevel::Warning: prefix = "[Warning] "; break;
            case LogLevel::Error:   prefix = "[Error] "; break;
        }
        
        fprintf(stderr, "%s", prefix);
        vfprintf(stderr, fmt, args);
        fprintf(stderr, "\n");
    }

    #ifdef GM_DEBUG
    static inline std::atomic<bool> s_debugEnabled{true};
    static inline std::atomic<bool> s_configured{false};

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
};

}} // namespace gm::core