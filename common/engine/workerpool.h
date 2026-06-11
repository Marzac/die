/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    pool of reusable threads
*/

#ifndef WORKERPOOL_H
#define WORKERPOOL_H

#include <vector>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>

/*****************************************************************************/
/**
    \brief Fixed pool of reusable worker threads
*/
class WorkerPool
{
public:
    static constexpr unsigned WorkersMax = 8;

    WorkerPool();
    ~WorkerPool();

    /// \brief Add a job to the pool (jobs may run in any order)
    void enqueue(std::function<void()> job);

    /// \brief Block until all enqueued jobs have completed
    void waitAll();

    /// \brief Number of worker threads in the pool
    int workerCount() const {return activeWorkers;}

private:
    std::vector<std::thread> workers;
    std::vector<std::function<void()>> jobs;

    std::mutex queueMutex;

    std::condition_variable jobCV;
    std::condition_variable waitCV;

    bool stop;
    int jobsPrepared;
    int jobsUnfinished;
    int activeWorkers;
};

#endif // WORKERPOOL_H
