#include <afina/concurrency/Executor.h>

namespace Afina {
namespace Concurrency {

void Executor::Start() {
    std::lock_guard<std::mutex> lock(mutex);
    state = State::kRun;
    for (int i = 0; i < low_watermark; i++) {
        std::thread([this]() { perform(this); }).detach();
    }
    created_threads = low_watermark;
    busy_threads = created_threads;
}

void Executor::Stop(bool await) {
    std::unique_lock<std::mutex> lock(mutex);
    state = State::kStopping;
    empty_condition.notify_all();
	
	if (await) {
        stop_work.wait(lock, [this] { return busy_threads == 0; });
        /*while (!threads.empty()) {
           stop_work.wait(lock);
        }*/
    }
    state = State::kStopped;
}

/**
 * Main function that all pool threads are running. It polls internal task queue and execute tasks
 */
void Executor::perform(Executor *executor) {
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
                executor->stop_work.notify_all();
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
}


}
} // namespace Afina
