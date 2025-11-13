#include "gm/utils/ThreadPool.hpp"

#include <algorithm>
#include <stdexcept>

namespace gm::utils {

ThreadPool::ThreadPool(std::size_t threadCount) {
    if (threadCount == 0) {
        threadCount = 1;
    }
    m_workers.reserve(threadCount);
    for (std::size_t i = 0; i < threadCount; ++i) {
        m_workers.emplace_back([this]() { WorkerLoop(); });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopping = true;
    }
    m_condition.notify_all();
    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::WorkerLoop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_condition.wait(lock, [this]() { return m_stopping || !m_tasks.empty(); });
            if (m_stopping && m_tasks.empty()) {
                return;
            }
            task = std::move(m_tasks.front());
            m_tasks.pop();
        }
        task();
    }
}

} // namespace gm::utils

