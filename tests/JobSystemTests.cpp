#include "jobs/JobSystem.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <mutex>
#include <vector>
#include <thread>

int main() {
    voxel::JobSystem jobs(2);

    auto value = jobs.submit(voxel::JobPriority::Normal, []() {
        return 42;
    });
    assert(value.get() == 42);

    auto failure = jobs.submit(voxel::JobPriority::Normal, []() -> int {
        throw 7;
    });
    try {
        (void)failure.get();
        assert(false);
    } catch (int value) {
        assert(value == 7);
    }

    voxel::JobSystem orderedJobs(1);
    std::promise<void> releaseWorker;
    std::shared_future<void> releaseFuture = releaseWorker.get_future().share();
    auto blocker = orderedJobs.submit(voxel::JobPriority::Normal, [releaseFuture]() {
        releaseFuture.wait();
    });

    std::mutex orderMutex;
    std::vector<int> order;
    auto low = orderedJobs.submit(voxel::JobPriority::Low, [&orderMutex, &order]() {
        std::lock_guard lock(orderMutex);
        order.push_back(1);
    });
    auto high = orderedJobs.submit(voxel::JobPriority::High, [&orderMutex, &order]() {
        std::lock_guard lock(orderMutex);
        order.push_back(2);
    });

    releaseWorker.set_value();
    blocker.get();
    low.get();
    high.get();
    assert((order == std::vector<int>{2, 1}));

    return 0;
}
