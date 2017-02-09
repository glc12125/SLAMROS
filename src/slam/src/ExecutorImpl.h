#ifndef INCLUDED_EXECUTORIMPL_H
#define INCLUDED_EXECUTORIMPL_H

#include "Executor.h"
#include "WorkerItem.h"
#include <mutex>

namespace Slam
{

class ExecutorImpl : public Executor
{
  public:
    ~ExecutorImpl(){ }

    ExecutorImpl();
    
    /// implementation of Executor::startCalculation
    virtual bool startCalculation(CalculationListener& listener);

};

} // close namepsace Slam

#endif