#ifndef AFINA_CONCURRENCY_EXECUTOR_H
#define AFINA_CONCURRENCY_EXECUTOR_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <chrono>
#include <iostream>


namespace Afina {
namespace Concurrency {
    /**
 * # Thread pool
 */


class Executor {
    enum class State {
        // Threadpool is fully operational, tasks could be added and get executed
        kRun,

        // Threadpool is on the way to be shutdown, no ned task could be added, but existing will be
        // completed as requested
        kStopping,

        // Threadppol is stopped
        kStopped
    };

public:


    Executor(int low_watermark, int hight_watermark, int max_queue_size, int  idle_time):
            low_watermark(low_watermark),
            hight_watermark(hight_watermark),
            max_queue_size(max_queue_size),
            idle_time(idle_time), created_threads(0) {}

    ~Executor() {
        Stop(true);
    }

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */
    void Stop(bool await);
    /**
     * Add function to be executed on the threadpool. Method returns true in case if task has been placed
     * onto execution queue, i.e scheduled for execution and false otherwise.
     *
     * That function doesn't wait for function result. Function could always be written in a way to notify caller about
     * execution finished by itself
     */
    template <typename F, typename... Types> bool Execute(F &&func, Types... args) {
        // Prepare "task"
        auto exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);

        std::unique_lock<std::mutex> lock(mutex);
        if (state != State::kRun || tasks.size() >= max_queue_size) {
            return false;
        }

        // Enqueue new task
        tasks.push_back(exec);
        if (busy_threads < created_threads) {
            empty_condition.notify_one();
        } else {
            if (busy_threads < hight_watermark) {
                ++created_threads;
                std::thread(
                    [this]() { perform(this); }
                           ).detach();
            }
        }
    }

    void Start();

private:
    // No copy/move/assign allowed
    Executor(const Executor &);            // = delete;
    Executor(Executor &&);                 // = delete;
    Executor &operator=(const Executor &); // = delete;
    Executor &operator=(Executor &&);      // = delete;

    /**
     * Main function that all pool threads are running. It polls internal task queue and execute tasks
     */
    void perform(Executor *executor);
    
    /**
     * Mutex to protect state below from concurrent modification
     */
    std::mutex mutex;

    /**
     * Conditional variable to await new data in case of empty queue
     */
    std::condition_variable empty_condition;
    std::condition_variable stop_condition;
    /**
     * Vector of actual threads that perorm execution
     */

    /**
     * Task queue
     */
    std::deque<std::function<void()>> tasks;

    /**
     * Flag to stop bg threads
     */
    State state;

    int low_watermark = 0;
    int hight_watermark = 0;
    int max_queue_size = 0;
    std::chrono::milliseconds idle_time;

    int created_threads = 0;
    int busy_threads = 0;


};

} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H
