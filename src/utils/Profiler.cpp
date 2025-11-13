#include "gm/utils/Profiler.hpp"

#include <algorithm>

namespace gm::utils {

Profiler& Profiler::Instance() {
    static Profiler instance;
    return instance;
}

void Profiler::BeginFrame() {
    std::lock_guard lock(m_mutex);
    m_current.samples.clear();
    m_frameStart = std::chrono::steady_clock::now();
}

void Profiler::EndFrame() {
    std::lock_guard lock(m_mutex);
    const auto now = std::chrono::steady_clock::now();
    m_current.frameTimeMs = std::chrono::duration<double, std::milli>(now - m_frameStart).count();
    m_last = m_current;
}

FrameProfile Profiler::GetLastFrame() const {
    std::lock_guard lock(m_mutex);
    return m_last;
}

Profiler::ScopedTimer::ScopedTimer(const std::string& name)
    : m_name(name)
    , m_start(std::chrono::steady_clock::now()) {
}

Profiler::ScopedTimer::~ScopedTimer() {
    const auto end = std::chrono::steady_clock::now();
    const double durationMs = std::chrono::duration<double, std::milli>(end - m_start).count();

    auto& profiler = Profiler::Instance();
    std::lock_guard lock(profiler.m_mutex);
    profiler.m_current.samples.push_back(ProfileSample{m_name, durationMs});
}

} // namespace gm::utils

