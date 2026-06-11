/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    pool of reusable threads
*/

#include "workerpool.h"

#include <algorithm>

/*****************************************************************************/
WorkerPool::WorkerPool() :
    stop(false),
    jobsPrepared(0),
    jobsUnfinished(0)
{
    unsigned count = std::max(1u, std::thread::hardware_concurrency());
    activeWorkers = (int) std::min(count, WorkersMax);

    for (int i = 0; i < activeWorkers; i++) {
        workers.emplace_back([this]() {
            while (true) {
                std::function<void()> job;
                {
                    std::unique_lock<std::mutex> lock(queueMutex);
                    jobCV.wait(lock, [this]() { return stop || jobsPrepared > 0; });
                    if (stop && jobsPrepared == 0) return;

                // Jobs run in LIFO order
                    job = std::move(jobs.back());
                    jobs.pop_back();
                    jobsPrepared--;
                }
                job();
                {
                    std::lock_guard<std::mutex> lock(queueMutex);
                    jobsUnfinished--;
                    if (jobsUnfinished == 0)
                        waitCV.notify_all();
                }
            }
        });
    }
}

WorkerPool::~WorkerPool()
{
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        stop = true;
    }
    jobCV.notify_all();
    for (auto& t : workers) t.join();
}

/*****************************************************************************/
void WorkerPool::enqueue(std::function<void()> job)
{
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        jobs.emplace_back(std::move(job));
        jobsPrepared++;
        jobsUnfinished++;
    }
    jobCV.notify_one();
}

void WorkerPool::waitAll()
{
    std::unique_lock<std::mutex> lock(queueMutex);
    waitCV.wait(lock, [this]() { return jobsPrepared == 0 && jobsUnfinished == 0; });
}
