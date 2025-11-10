#pragma once
#include <cstdio>
#include <string>
#include <cstdarg>

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
};

}} // namespace gm::core