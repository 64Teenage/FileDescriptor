#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

#include <queue>
#include <vector>
#include <iostream>
#include <thread>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <future>

#include "util.h"



class ThreadPool {
private:
    using Task = std::function<void()>;

public:
    static ThreadPool* getInstance(const unsigned int nthreads) {
        static ThreadPool instance(nthreads);
        return &instance;
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

        submit(threadFunc);


        return task_ptr->get_future();
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mTaskLock);
            mFinish = true;
            mTaskCond.notify_all();
        }

        for(auto & element : mWorkers) {
            if(element.joinable()) {
                element.join();
            }
        }
        
    }

private:
    ThreadPool(const unsigned int nthreads): mFinish(false) {
        int nThreads = nthreads > std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : nthreads;
        //int nThreads = static_cast<int>(std::thread::hardware_concurrency() / 64.0 * 60);
        for(int pos = 0; pos < nThreads; ++pos) {
            mWorkers.push_back(std::thread(&ThreadPool::worker, this));
        }
    }



    void    worker() {
        while(true) {
            //bool isGet = false;
            Task task;
            {
                std::unique_lock<std::mutex> lock(mTaskLock);
                mTaskCond.wait(lock, [&](){return !mTaskQueue.empty() || mFinish;});
                if(mTaskQueue.empty() /*&& mFinish*/) {
                    return ;
                }

                task = mTaskQueue.front();
                mTaskQueue.pop();
            }          

            task();

        }
    }

    void    submit(const Task & task) {
        std::lock_guard<std::mutex> lock(mTaskLock);
        mTaskQueue.push(task);
        mTaskCond.notify_one();
    }
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool& operator=(const ThreadPool &) = delete;

private:
    std::vector<std::thread>    mWorkers;
    std::queue<Task>            mTaskQueue;

    bool                    mFinish;
    std::mutex              mTaskLock;
    std::condition_variable mTaskCond;
};

#endif
