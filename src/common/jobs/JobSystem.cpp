#include "jobs/JobSystem.hpp"

#include <algorithm>

namespace voxel {

std::size_t JobSystem::defaultWorkerCount() {
    const unsigned int hardwareThreads = std::thread::hardware_concurrency();
    if (hardwareThreads <= 2) {
        return 1;
    }
    return std::min<std::size_t>(static_cast<std::size_t>(hardwareThreads - 1), 6);
}

JobSystem::JobSystem(const std::size_t workerCount) {
    const std::size_t clampedWorkerCount = std::max<std::size_t>(1, workerCount);
    workers_.reserve(clampedWorkerCount);
    for (std::size_t i = 0; i < clampedWorkerCount; ++i) {
        workers_.emplace_back([this]() { workerLoop(); });
    }
}

JobSystem::~JobSystem() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    wake_.notify_all();
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

bool JobSystem::JobCompare::operator()(const Job& lhs, const Job& rhs) const {
    if (lhs.priority != rhs.priority) {
        return static_cast<int>(lhs.priority) < static_cast<int>(rhs.priority);
    }
    return lhs.sequence > rhs.sequence;
}

void JobSystem::enqueue(JobPriority priority, std::function<void()> run) {
    {
        std::lock_guard lock(mutex_);
        jobs_.push({priority, nextSequence_++, std::move(run)});
    }
    wake_.notify_one();
}

void JobSystem::workerLoop() {
    while (true) {
        Job job;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, [this]() { return stopping_ || !jobs_.empty(); });
            if (stopping_ && jobs_.empty()) {
                return;
            }
            job = jobs_.top();
            jobs_.pop();
        }
        job.run();
    }
}

}  // namespace voxel
