#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <memory>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <type_traits>  // result_of 所需头文件

namespace ning {
namespace maps {
namespace ocm {

class ThreadPool {
public:
    explicit ThreadPool(size_t threads);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<
            typename std::decay<F>::type(typename std::decay<Args>::type...)
        >::type>
    {
        using return_type = typename std::result_of<
            typename std::decay<F>::type(typename std::decay<Args>::type...)
        >::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);

            if (stop)
                throw std::runtime_error("enqueue on stopped ThreadPool");

            tasks.emplace([task]() { (*task)(); });
        }
        condition.notify_one();
        return res;
    }

    void shutdown();

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

} // namespace ocm
} // namespace maps
} // namespace ning

#endif // THREAD_POOL_HPP