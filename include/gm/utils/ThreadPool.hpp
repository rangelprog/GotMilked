#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace gm::utils {

class ThreadPool {
public:
    explicit ThreadPool(std::size_t threadCount = std::thread::hardware_concurrency());
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    std::size_t ThreadCount() const { return m_workers.size(); }

    template <typename Func, typename... Args>
    auto Submit(Func&& func, Args&&... args)
        -> std::future<std::invoke_result_t<Func, Args...>> {
        using ResultType = std::invoke_result_t<Func, Args...>;

        auto task = std::make_shared<std::packaged_task<ResultType()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

        std::future<ResultType> future = task->get_future();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stopping) {
                throw std::runtime_error("ThreadPool::Submit on stopped pool");
            }
            m_tasks.emplace([task]() { (*task)(); });
        }
        m_condition.notify_one();
        return future;
    }

private:
    void WorkerLoop();

    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    bool m_stopping = false;
};

} // namespace gm::utils

