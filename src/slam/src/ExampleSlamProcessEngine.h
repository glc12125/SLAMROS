#ifndef INCLUDED_EXAMPLESLAMPROCESSENGINE_H
#define INCLUDED_EXAMPLESLAMPROCESSENGINE_H

#include "SlamProcessEngine.h"
#include "WorkerItem.h"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace Slam
{

class ExampleSlamProcessEngine : public SlamProcessEngine,
                                 public WorkerThreadClientBase<ExampleSlamProcessEngine>
{
  public:
    ~ExampleSlamProcessEngine () {}

    ExampleSlamProcessEngine ();
    /// implementation of SlamProcessEngine::queueRequest
    virtual bool queueRequest(SlamProcessEngine::NewDataListener& listener);
    
  private:
    /// implementation of SlamProcessEngine::startProcessData
    virtual bool startProcessData(SlamProcessEngine::NewDataListener& listener);
    
  private:
    class ExampleSlamWorkerItem : public WorkerItem<ExampleSlamProcessEngine>
    {
      public:
        ExampleSlamWorkerItem(SlamProcessEngine::NewDataListener& listener) :
            m_listener(listener) 
        { }

        /// Implementation of WorkerItem::action
        virtual bool action(ExampleSlamWorkerItem& owner)
        { 
            return owner.startProcessData(m_listener); 
        }

      private:
        SlamProcessEngine::NewDataListener& m_listener;
    };

    friend class ExampleSlamWorkerItem;
    
    //std::mutex m_queryTSMutex;
    //std::condition_variable m_queryTSCondition;
    double m_currentTimestamp;
};

} // Slam

#endif
