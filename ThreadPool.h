#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

#include <queue>
#include <vector>
#include <unordered_set>
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

    bool    adjust(const unsigned int threads) {
        auto maxThreads = std::thread::hardware_concurrency();
        if(threads > maxThreads) {
            DEG_LOG("%d is bigger than hardware support: %d", threads, maxThreads);
            return false;
        }

        unsigned nowThreads = mWorkers.size();
        if(nowThreads > threads)  {
            subtract(nowThreads - threads);
        } else if(nowThreads < threads) {
            append(threads - nowThreads);
        }

        DEG_LOG("thread num: %d", mWorkers.size());

        return true;
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
        DEG_LOG("Thread Service exit ...");
        
    }

private:
    ThreadPool(const unsigned int nthreads): mFinish(false), mAdjust(false) {
        int nThreads = nthreads > std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : nthreads;
        //int nThreads = static_cast<int>(std::thread::hardware_concurrency() / 64.0 * 60);
        for(int pos = 0; pos < nThreads; ++pos) {
            mWorkers.push_back(std::thread(&ThreadPool::worker, this ));
        }
    }

    void    append(const  int threads) {
        for(int indx = 0; indx < threads; ++indx) {
            mWorkers.push_back(std::thread(&ThreadPool::worker, this));
        }
    }

    void    subtract(const  int threads) {
        {
            std::lock_guard<std::mutex> lock(mTaskLock);
            mModifyThreads = threads;
            mAdjust        = true;
            mTaskCond.notify_all();
        }

        {
            std::unique_lock<std::mutex> lock(mTaskLock);
            mModifyCond.wait(lock, [&](){return mModifyThreads == 0;});
        }

        for(const auto & element : mRecycleThreads) {
            DEG_LOG("Ready to remove: %d", element);
        }
        for(auto it = mWorkers.begin(); it != mWorkers.end();) {
            if(mRecycleThreads.count(it->get_id()) > 0) {
                if(it->joinable()) {
                    it->join();
                }
                it = mWorkers.erase(it);
            } else {
                ++it;
            }
        }

        mRecycleThreads.clear();
        {
            std::lock_guard<std::mutex> lock(mTaskLock);
            mAdjust = false;
        }
    }


    void    worker() {
        while(true) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(mTaskLock);
                mTaskCond.wait(lock, [&](){return !mTaskQueue.empty() || mFinish || mAdjust;});
                if(mAdjust) {
                    if(mModifyThreads > 0) {
                        --mModifyThreads;
                        DEG_LOG("remove thread: %ld", std::this_thread::get_id());
                        mRecycleThreads.insert(std::this_thread::get_id());
                        if(mModifyThreads == 0) {
                            mModifyCond.notify_one();
                        }
                        return ;
                    } else {
                        continue;
                    }
                }

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
    std::vector<std::thread> mWorkers;
    std::queue<Task>        mTaskQueue;

    bool                    mFinish;
    std::mutex              mTaskLock;
    std::condition_variable mTaskCond;

    int                                 mModifyThreads;
    bool                                mAdjust;
    std::condition_variable             mModifyCond;
    std::unordered_set<std::thread::id> mRecycleThreads;
};

#endif
