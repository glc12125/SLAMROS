#ifndef INCLUDED_WORKERITEM_H
#define INCLUDED_WORKERITEM_H

#include "ConcurrentQueue.h"

#include <iostream>
#include <memory>

namespace MathLib {

//Abstract base class for all thread worker items
template<typename T>
class WorkerItem
{
  public:
    WorkerItem() : v_completed(false), v_success(false) {}
    virtual ~WorkerItem () {}
    
    // This calls action and indicate result with v_success
    void invoke(T& owner);
    
    // Called by third party thread to wait for completion
    bool wait(int64_t waitMilliseconds = -1);
    
  protected:
    // Invoke this to perform relevant actions
    virtual bool action(T& owner) = 0;
    
  private:
    WorkerItem(const WorkerItem&) = delete;             // Disable copy constructor
    WorkerItem& operator=(const WorkerItem&) = delete;  // Disable copy assignment operator
    
    bool v_completed;
    bool v_success;
    std::mutex v_mutex;
    std::condition_variable v_cond;
};

void workerThreadMainWrapper(void *);

class WorkerThreadBase
{
  public:
    virtual void workerThreadMain() = 0;
};

/// Abstract base class for all worker threads
template<typename T>
    class WorkerThread : public WorkerThreadBase
{
  public:
    /// create a worker thread used by owner and default high water mark of 100;
    /// if queue size is less, the queue does not block when adding items.
    /// if it is more, addding new items is blocked until queue falls below that size
    WorkerThread(T& owner, int queueSize=100);
    virtual ~WorkerThread();
    
    virtual void workerThreadMain();
        
    /// queue work item. If time (in seconds) is < 0, it blocks until item is queued,
    /// otherwise times out
    bool queueWorkerItem(std::shared_ptr<WorkerItem<T> > item, int timeOutSec = -1);
        
    void stop() { v_stopping = true; }
  private:
    class StopWorkerItem : public WorkerItem<T>
    {
      public:
        virtual bool action(T& owner)
        {
            // owner.v_stopping = true;
            owner.stop();
            return true;
        }
    };
        
    // Declare all the WorkerItem derivations as friends so they can access the
    // private methods.
    friend class StopWorkerItem;
        
    /// suppress copy ctor and assignment (as there is a ref member)
    WorkerThread(const WorkerThread&) = delete;
    WorkerThread& operator=(const WorkerThread&) = delete;
        
    /// Indicates the worker thread should terminate.
    bool v_stopping;

    /// Worker thread work queue.
    ConcurrentQueue<std::shared_ptr<WorkerItem<T> > > v_workQueue;
        
    /// @brief the object that requires this thread and implements operations
    ///        for work item
    T&    v_owner;
    
    /// The Thread that takes item out of the queue
    std::thread v_workerItemAllocator;
};

/// base class for clients using worker thread.
/// Worker thread requires this functionality
template<typename T>
class WorkerThreadClientBase
{
  public:
    virtual ~WorkerThreadClientBase() {}
    void setThreadObj(WorkerThread<T>* thrObj) { v_workerThread = thrObj; }
    void stop() { v_workerThread->stop(); }
    /// queue work item in a queue. If time out is < 0, the call is blocked until it succeeds.
    bool queueWorkerItem(std::shared_ptr<WorkerItem<T> > item, int timeOut = THREAD_QUEUE_TIMEOUT_SECS)
    {
        return v_workerThread->queueWorkerItem(item, timeOut);
    }
        
  protected:
    WorkerThread<T>*  v_workerThread;
    /// Overall timeout in seconds for runOnRecentInstances call.
    static const int THREAD_QUEUE_TIMEOUT_SECS = 10;
};
    
    
template<typename T>
WorkerThread<T>::WorkerThread(T& owner, int queueSize) : v_stopping(false), v_workQueue(queueSize), v_owner(owner)
{
    // set thread object for T
    WorkerThreadClientBase<T>& ownerBase = static_cast<T&>(owner);
    ownerBase.setThreadObj(this);
    
    v_workerItemAllocator = std::thread(workerThreadMainWrapper, this);
}
    
    
template<typename T>
void WorkerThread<T>::workerThreadMain()
{
    std::cout << "WorkerThread<T>::workerThreadMain()\n";
        
    // Main loop - process items from work queue until we're told to stop.
    std::cout << "WorkerThread: entering loop\n";
    while (!v_stopping) {
        std::shared_ptr<WorkerItem<T> > item = v_workQueue.pop();
        std::cout << "WorkerThread: starting work item\n";
        item->invoke(v_owner);
        std::cout << "WorkerThread: work item completed\n";
    }
        
    std::cout << "WorkerThread: returning from thread function" << std::endl;
}
    
    
    
template<typename T>
WorkerThread<T>::~WorkerThread()
{

    std::cout << "~WorkerThread()\n";
        
    std::shared_ptr<WorkerItem<T> > stopWorkerItem(new StopWorkerItem);
    bool stopped = queueWorkerItem(stopWorkerItem);
    if (stopped) {
        // Wait for 10 seconds for the item to be actioned.
        std::cout << "waiting for stop\n";
        stopped = stopWorkerItem->wait(-1); // FIXME: previously 10000, but if a queue is blocked and this fails finally, thread won't stop
    } else {
        // If we fail to even queue the StopWorkItem then there's no way that
        // it's going to be actioned.
        std::cout << "failed to queue stop\n";
    }
        
    // Only attempt to join if stop was successful - chances are we're shutting
    // down if this destructor is called, and failing to join with a thread is
    // probably less likely to cause issues than hanging forever in a join.
    if (stopped) {
        std::cout << "joining worker thread\n";
        if(v_workerItemAllocator.joinable()){
            v_workerItemAllocator.join();
        }
    } else {
        std::cout << "failed to stop thread\n";
    }
    std::cout << "Quitting WorkerThread<T>::~WorkerThread)" << std::endl;
}
    
    
    
template<typename T>
bool WorkerThread<T>::queueWorkerItem(std::shared_ptr<WorkerItem<T> > item, int timeOut)
{
    bool rc = 0;
    if (timeOut < 0)
    {
        v_workQueue.push(item);
        return true;
    }
    else
    {
        rc = v_workQueue.timedPush(item, timeOut);
    }
    if (rc) {
        return true;
    } else {
        std::cout << "queueWorkerItem(): timeout" << std::endl;
        return false;
    }
}
    
    
template<typename T>
void WorkerItem<T>::invoke(T& owner)
{
    bool success = action(owner);
        
    std::unique_lock<std::mutex> mlock(v_mutex);
    v_success = success;
    v_completed = true;
    v_cond.notify_all();
}
    
    
/// @param[in]  waitMilliseconds        number of milliseconds to wait
///
/// @return     false if timeout reached, success of action otherwise
template<typename T>
bool WorkerItem<T>::wait(int64_t waitMilliseconds)
{
    std::cout << "WorkerItem::wait(" << waitMilliseconds << ")\n";
    
    std::unique_lock<std::mutex> lockGuard(v_mutex);
    while (!v_completed) {
        if (waitMilliseconds >= 0) {
            std::cv_status rc = v_cond.wait_for(lockGuard, std::chrono::milliseconds(waitMilliseconds));
            if (rc == std::cv_status::timeout) {
                std::cout << "WorkerItem::wait(): timedWait() returned TIMEOUT" << "\n";
            } else {
                std::cout << "WorkerItem::wait(): timedWait() returned NO_TIMEOUT" << "\n";
            }
        } else {
            v_cond.wait(lockGuard);
        }
    }
    bool success = false;
    if (v_completed) {
        success = v_success;
    }
        
    return success;
}
    
}// End of MathLib

#endif /* defined(INCLUDED_WORKERITEM_H) */
