//
//  ConcurrentQueue.h
//  dtw
//
//  Created by Liangchuan Gu on 18/05/2015.
//  Copyright (c) 2015 gao chao. All rights reserved.
//

#ifndef INCLUDED_CONCURRENTQUEUE_H
#define INCLUDED_CONCURRENTQUEUE_H

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <iostream>

namespace MathLib {

template<typename T>
class ConcurrentQueue
{
  public:
    T pop () {
        std::unique_lock<std::mutex> mlock(v_mutex);
        while (v_queue.empty())
        {
            v_cond.wait(mlock);
        }
        auto val = v_queue.front();
        v_queue.pop();
        --v_size;
        
        std::cout << "Current workeritem in the queue: "
        << v_size << ", capacity: "
        << v_capacity
        << std::endl;
        return val;
    }
    
    bool timedPop (T& t, int seconds) {
        std::unique_lock<std::mutex> mLock(v_mutex);

        if(!v_cond.wait_for(mLock,
                            std::chrono::seconds(seconds),
                            [this]{return !v_queue.empty();})){
            return false;
        }

        auto val = v_queue.front();
        v_queue.pop();
        --v_size;
        
        std::cout << "Current workeritem in the queue: "
        << v_size << ", capacity: "
        << v_capacity
        << std::endl;
        t = val;
        v_cond.notify_one();
        return true;
    }
    
    void push(const T& item)
    {
        std::unique_lock<std::mutex> mlock(v_mutex);
        v_queue.push(item);
        ++v_size;
        mlock.unlock();
        
        std::cout << "Current workeritem in the queue: "
        << v_size << ", capacity: "
        << v_capacity
        << std::endl;
        v_cond.notify_one();
    }
    
    bool timedPush(const T& item, int seconds)
    {
        std::unique_lock<std::mutex> mlock(v_mutex);
        if (
            v_cond.wait_for(mlock,
                            std::chrono::seconds(seconds),
                            [this]{return v_capacity > v_size;})
        ) {
            v_queue.push(item);
            ++v_size;
            
            std::cout << "Current workeritem in the queue: "
                      << v_size << ", capacity: "
                      << v_capacity
                      << std::endl;
            v_cond.notify_one();
            return true;
        } else {
            return false;
        }
    }
    
    ConcurrentQueue(int capacity):
        v_size(0),
        v_capacity(capacity)
    {}
    
    ConcurrentQueue():
        v_size(0),
        v_capacity(100)
    {}
    ConcurrentQueue(const ConcurrentQueue&) = delete;            // disable copy constructor
    ConcurrentQueue& operator=(const ConcurrentQueue&) = delete; // disable copy assignment operator
  
  private:
    std::queue<T> v_queue;
    std::mutex v_mutex;
    std::condition_variable v_cond;
    int v_size;
    int v_capacity;

};

} // End of MathLib

#endif
