#ifndef AFINA_CONCURRENCY_EXECUTOR_H
#define AFINA_CONCURRENCY_EXECUTOR_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
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
    Executor(std::string name, 
            int low_watermark, 
            int high_watermark, 
            int max_queue_size, 
            int idle_time) : 
        low_watermark(low_watermark),
        high_watermark(high_watermark), 
        max_queue_size(max_queue_size),
        idle_time(idle_time),
        created_threads(0),
        busy_threads(0) 
        {
            
        }
        
    ~Executor() {
        Stop(true);
    };

    void Start();
    /*void Start() {
        std::lock_guard<std::mutex> lock(mutex);
        state = State::kRun;
        for (int i = 0; i < low_watermark; i++) {
            std::thread([this]() { perform(this); }).detach();
        }
        created_threads = low_watermark;
        busy_threads = created_threads;
    }*/

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */
    void Stop(bool await = false);
    /*void Stop(bool await = false) {
        std::unique_lock<std::mutex> lock(mutex);
        state = State::kStopping;
        empty_condition.notify_all();

        if (await) {
            stop_work.wait(lock, [this] { return busy_threads == 0; });
            /
        }
        state = State::kStopped;
    }*/

    /**
     * Add function to be executed on the threadpool. Method returns true in case if task has been placed
     * onto execution queue, i.e scheduled for execution and false otherwise.
     *
     * That function doesn't wait for function result. Function could always be written in a way to notify caller about
     * execution finished by itself
     */
    template <typename F, typename... Types> bool Execute(F &&func, Types... args) {

        if (state != State::kRun || created_threads >= max_queue_size) {
            return false;
        }
        // Prepare "task"
        auto exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);

        std::unique_lock<std::mutex> lock(this->mutex);
        /*if (state != State::kRun) {
            return false;
        }*/
    
        if (busy_threads < created_threads) {
            tasks.push_back(exec);
            empty_condition.notify_one();
            return true;
        }
        if (created_threads < high_watermark) {
            std::thread([this]() {
                perform(this);
            }).detach();
            tasks.push_back(exec);
            ++busy_threads;
            return true;
        }
        if (tasks.size() < max_queue_size) {
            tasks.push_back(exec);
            return true;
        }
        return false;
        /*if (busy_threads < created_threads && created_threads < high_watermark) {
            std::thread([this]() {
                perform(this);
            }).detach();
            tasks.push_back(exec);
            ++created_threads;
            return true;
        }
        if (busy_threads == 0 && created_threads == size) {
            if (queue.size() < max_queue_size) {
                tasks.push_back(exec);    
            } else {
                return false;
            }
            return true;
        }
        if (busy_threads == 0 && queue.size() < max_queue_size) {

        }
        if (queue.size() == max_queue_size) {
            return false;
        }
        // Enqueue new task
        tasks.push_back(exec);
        empty_condition.notify_one();
        return true;
        */
    }

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
    /*void perform(Executor *executor) {
        std::function<void()> task;
        while (true) {
            std::unique_lock<std::mutex> lock(executor->mutex);

            auto pred = [executor] { 
                return !executor->tasks.empty() && executor->state == Executor::State::kRun; 
            };
            bool result = executor->empty_condition.wait_for(lock, executor->idle_time, pred);
            if (result) {
                if ((executor->state == Executor::State::kRun || (executor->state == Executor::State::kStopping))
                        && !executor->tasks.empty()) {
                    task = executor->tasks.front();
                    executor->tasks.pop_front();
                    executor->busy_threads++;
                } else if (busy_threads == size) {
                    executor->state = Executor::State::kStopped;
                    executor->stop_work.notify_one();
                }
                break;
            } else if (executor->low_watermark < executor->created_threads) {
                executor->created_threads--;
                break;
            }

            try {
                task();
            } catch (std::exception &e) { 
                std::cout << e.what() << std::endl; 
            }

        }
    }*/
    /**
     * Mutex to protect state below from concurrent modification
     */
    std::mutex mutex;

    /**
     * Conditional variable to await new data in case of empty queue
     */
    std::condition_variable empty_condition;

    /**
     * Counter of actual threads that perform execution
     */
    int created_threads;

    /**
     * Task queue
     */
    std::deque<std::function<void()>> tasks;

    /**
     * Flag to stop bg threads
     */
    State state;

    int low_watermark;
    int high_watermark;
    int size;
    std::chrono::milliseconds idle_time;
    int max_queue_size;
    int busy_threads;

    std::condition_variable stop_work;
};

} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H
