#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/Async/BackgroundWorker.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

using HedgehogEngine::BackgroundWorker;

TEST_CASE("BackgroundWorker - jobs run in submission order, off the calling thread")
{
    BackgroundWorker  worker;
    std::mutex        mutex;
    std::vector<int>  order;
    std::thread::id   jobThread;
    BackgroundWorker::JobId last = 0;
    for (int i = 0; i < 100; ++i)
    {
        last = worker.Submit([&, i](std::stop_token)
        {
            std::lock_guard lock(mutex);
            order.push_back(i);
            jobThread = std::this_thread::get_id();
        });
    }
    worker.Wait(last);
    CHECK(worker.IsDone(1));
    CHECK(worker.IsDone(last));
    CHECK_FALSE(worker.IsDone(last + 1));
    REQUIRE(order.size() == 100);
    for (int i = 0; i < 100; ++i)
        CHECK(order[i] == i);
    CHECK(jobThread != std::this_thread::get_id());
}

TEST_CASE("BackgroundWorker - Stop skips queued jobs and lets the running one see the stop")
{
    BackgroundWorker  worker;
    std::atomic<bool> started{ false };
    std::atomic<bool> sawStop{ false };
    std::atomic<int>  laterRan{ 0 };

    const auto running = worker.Submit([&](std::stop_token stop)
    {
        started = true;
        while (!stop.stop_requested())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        sawStop = true;
    });
    BackgroundWorker::JobId queued = 0;
    for (int i = 0; i < 10; ++i)
        queued = worker.Submit([&](std::stop_token) { ++laterRan; });
    while (!started)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    CHECK_FALSE(worker.IsDone(running));

    worker.Stop();
    CHECK(worker.IsStopped());
    CHECK(sawStop);
    CHECK(laterRan == 0);
    CHECK(worker.IsDone(running));
    CHECK(worker.IsDone(queued));

    // A job submitted afterwards never runs, and is done at once.
    const auto after = worker.Submit([&](std::stop_token) { ++laterRan; });
    CHECK(worker.IsDone(after));
    worker.Wait(after);
    worker.Stop(); // safe twice
    CHECK(laterRan == 0);
}

TEST_CASE("BackgroundWorker - destroying it while a job runs joins without a hang")
{
    auto              worker  = std::make_unique<BackgroundWorker>();
    std::atomic<bool> started{ false };
    std::atomic<bool> queuedRan{ false };
    worker->Submit([&](std::stop_token stop)
    {
        started = true;
        while (!stop.stop_requested())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    });
    worker->Submit([&](std::stop_token) { queuedRan = true; });
    while (!started)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    worker.reset();
    CHECK(started);
    CHECK_FALSE(queuedRan);
}

TEST_CASE("BackgroundWorker - a job that throws is done and the next one runs")
{
    BackgroundWorker  worker;
    std::atomic<bool> ran{ false };
    const auto failing = worker.Submit([](std::stop_token) { throw std::runtime_error("broken asset"); });
    const auto next    = worker.Submit([&](std::stop_token) { ran = true; });
    worker.Wait(next);
    CHECK(worker.IsDone(failing));
    CHECK(ran);
}
