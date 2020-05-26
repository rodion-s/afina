#include <afina/concurrency/Executor.h>
#include <atomic>
#include <memory>


namespace Afina {
namespace Concurrency {

	void Executor::Start() {
        std::unique_lock<std::mutex> lock(mutex);
        state = State::kRun;
        for (int i = 0; i < low_watermark; ++i) {
            std::thread(
            	[this]() { perform(this); }
            			).detach();
            ++created_threads;
        }
    }

	void Executor::Stop(bool await) {
        std::unique_lock<std::mutex> lock(mutex);
        state = State::kStopping;
        if (await && created_threads > 0) {
            stop_condition.wait(lock);
            state = State::kStopped;        
        } else if (created_threads == 0) {
            state = State::kStopped;        
        }
    }

    void Executor::perform(Executor *executor) {
        try {        
            std::function<void()> task;
            for (;;) {
                
                std::unique_lock<std::mutex> lock(executor->mutex);
                bool res = executor->empty_condition.wait_until(lock, std::chrono::system_clock::now() + executor->idle_time,
                                                          [executor] { 
                                                          	return !(executor->tasks.empty()) || (executor->state != Executor::State::kRun);
                                                          });
                

                if (res) {
                    if ((executor->state == Executor::State::kRun || executor->state == Executor::State::kStopping)
                     && !executor->tasks.empty()) {
                        task = executor->tasks.front();
                        executor->tasks.pop_front();
                        executor->busy_threads++;
                        lock.unlock();
                        task();
                        lock.lock();
                        executor->busy_threads--;
                    } else {
                        break;
                    }
                } else if (executor->created_threads > executor->low_watermark) {
                    break;
                }
            }
            
            std::unique_lock<std::mutex> lock(executor->mutex);
            executor->created_threads--;
            if (executor->state == Executor::State::kStopping 
            		&& executor->created_threads == 0) {
            	executor->state = Executor::State::kStopped;
                executor->stop_condition.notify_all();
        	}
        } catch (std::exception &e) { 
        	std::cout << e.what() << std::endl; 
        }
    }


}
} // namespace Afina

