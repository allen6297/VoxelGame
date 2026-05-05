#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "world/Chunk.hpp"
#include "world/TerrainGenerator.hpp"

namespace voxel {

struct ChunkTask {
    ChunkCoord coord;
    std::function<void(Chunk)> callback;
};

class ChunkTaskQueue {
public:
    explicit ChunkTaskQueue(const TerrainGenerator& terrainGen, const GameData& gameData, int threadCount = 2)
        : terrainGen_(terrainGen), gameData_(gameData), stop_(false) {
        for (int i = 0; i < threadCount; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    ChunkTask task;
                    {
                        std::unique_lock<std::mutex> lock(queueMutex_);
                        condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                        if (stop_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }

                    Chunk chunk = terrainGen_.generateChunk(task.coord, gameData_);

                    {
                        std::lock_guard<std::mutex> lock(resultsMutex_);
                        results_.push({task.coord, std::move(chunk), task.callback});
                    }
                }
            });
        }
    }

    ~ChunkTaskQueue() {
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            stop_ = true;
        }
        condition_.notify_all();
        for (auto& worker : workers_) {
            worker.join();
        }
    }

    void enqueue(const ChunkCoord& coord, std::function<void(Chunk)> callback) {
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            tasks_.push({coord, callback});
        }
        condition_.notify_one();
    }

    void poll() {
        std::vector<Result> localResults;
        {
            std::lock_guard<std::mutex> lock(resultsMutex_);
            while (!results_.empty()) {
                localResults.push_back(std::move(results_.front()));
                results_.pop();
            }
        }

        for (auto& res : localResults) {
            if (res.callback) {
                res.callback(std::move(res.chunk));
            }
        }
    }

private:
    struct Result {
        ChunkCoord coord;
        Chunk chunk;
        std::function<void(Chunk)> callback;
    };

    const TerrainGenerator& terrainGen_;
    const GameData& gameData_;
    std::vector<std::thread> workers_;
    std::queue<ChunkTask> tasks_;
    std::queue<Result> results_;
    std::mutex queueMutex_;
    std::mutex resultsMutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_;
};

} // namespace voxel
