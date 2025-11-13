#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace gm::utils {

struct ProfileSample {
    std::string name;
    double durationMs = 0.0;
};

struct FrameProfile {
    double frameTimeMs = 0.0;
    std::vector<ProfileSample> samples;
};

class Profiler {
public:
    static Profiler& Instance();

    void BeginFrame();
    void EndFrame();

    FrameProfile GetLastFrame() const;

    class ScopedTimer {
    public:
        ScopedTimer(const std::string& name);
        ~ScopedTimer();

    private:
        std::string m_name;
        std::chrono::steady_clock::time_point m_start;
    };

private:
    Profiler() = default;

    mutable std::mutex m_mutex;
    FrameProfile m_current;
    FrameProfile m_last;
    std::chrono::steady_clock::time_point m_frameStart{};
};

} // namespace gm::utils

