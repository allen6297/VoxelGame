#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace voxel {

enum class JobPriority {
    Low = 0,
    Normal = 1,
    High = 2
};

class JobSystem {
public:
    explicit JobSystem(std::size_t workerCount = defaultWorkerCount());
    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;
    ~JobSystem();

    static std::size_t defaultWorkerCount();

    template <typename Func>
    auto submit(JobPriority priority, Func&& func) -> std::future<std::invoke_result_t<std::decay_t<Func>&>> {
        using Result = std::invoke_result_t<std::decay_t<Func>&>;

        auto promise = std::make_shared<std::promise<Result>>();
        std::future<Result> future = promise->get_future();
        auto task = std::make_shared<std::decay_t<Func>>(std::forward<Func>(func));

        enqueue(priority, [promise, task]() mutable {
            try {
                if constexpr (std::is_void_v<Result>) {
                    (*task)();
                    promise->set_value();
                } else {
                    promise->set_value((*task)());
                }
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });

        return future;
    }

private:
    struct Job {
        JobPriority priority = JobPriority::Normal;
        std::size_t sequence = 0;
        std::function<void()> run;
    };

    struct JobCompare {
        bool operator()(const Job& lhs, const Job& rhs) const;
    };

    void enqueue(JobPriority priority, std::function<void()> run);
    void workerLoop();

    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::priority_queue<Job, std::vector<Job>, JobCompare> jobs_;
    std::vector<std::thread> workers_;
    std::size_t nextSequence_ = 0;
    bool stopping_ = false;
};

}  // namespace voxel
