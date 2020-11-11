#ifndef HANDLERTHREAD_H_
#define HANDLERTHREAD_H_

#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>

#include <functional>
#include <queue>

class HandlerThread {
private:
    using Task = std::function<void()>;

public:
    HandlerThread(): mForceQuit(false), mSafeQuit(false) {
        mWorker = std::thread([&](){
            while(true) {
                Task task;
                {
                    std::unique_lock<std::mutex> lock(mEnqueueLock);
                    mTaskCond.wait(lock,[&](){return !mTaskQueue.empty()|| mForceQuit || mSafeQuit;});
                    if(mTaskQueue.empty() || mForceQuit) {
                        return ;
                    }

                    task = mTaskQueue.front();
                    mTaskQueue.pop();
                }
                task();
            }
        });
    }

    template<typename F, typename... Args>
    auto    enqueue(F && f, Args &&... args) -> std::shared_future<decltype(f(args...))> {
        using RType = decltype(f(args...));
        std::function<RType()> func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

        auto task_ptr = std::make_shared<std::packaged_task<RType()>>(func);
        std::function<void()> threadFunc = [task_ptr]() {
            (*task_ptr)();
            return ;
        };

        {
            std::lock_guard<std::mutex> lock(mEnqueueLock);
            mTaskQueue.push(threadFunc);
            mTaskCond.notify_one();
        }


        return task_ptr->get_future();
    }

    void    quit() {
        {
            std::lock_guard<std::mutex> lock(mEnqueueLock);
            mSafeQuit = true;
        }
        mTaskCond.notify_one();

        if(mWorker.joinable()) {
            mWorker.join();
        }
    }
    void    quitSafely() {
        {
            std::lock_guard<std::mutex> lock(mEnqueueLock);
            mForceQuit = true;
        }
        mTaskCond.notify_one();

        if(mWorker.joinable()) {
            mWorker.join();
        }
    }

    ~HandlerThread() {
        quit();
    }


private:
    HandlerThread(const HandlerThread &) = delete;
    HandlerThread& operator=(const HandlerThread &) = delete;

private:
    bool    mForceQuit;
    bool    mSafeQuit;
    std::thread             mWorker;
    std::mutex              mEnqueueLock;
    std::condition_variable mTaskCond;
    std::queue<Task>        mTaskQueue;

};

#endif
