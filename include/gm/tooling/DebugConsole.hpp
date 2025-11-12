#pragma once

#if defined(GM_DEBUG) || defined(_DEBUG)

#include <vector>
#include <string>
#include <chrono>

#include "gm/core/Logger.hpp"

namespace gm::tooling {

class DebugConsole {
public:
    DebugConsole();
    ~DebugConsole();

    void Render(bool* open);
    void Clear();

private:
    struct Entry {
        core::LogLevel level;
        std::string message;
        std::chrono::system_clock::time_point timestamp;
    };

    std::vector<Entry> m_entries;
    bool m_autoScroll = true;
    bool m_scrollToBottom = false;
    size_t m_listenerToken = 0;
};

} // namespace gm::tooling

#endif // GM_DEBUG || _DEBUG

