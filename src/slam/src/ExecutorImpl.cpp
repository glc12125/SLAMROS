#include "ExecutorImpl.h"

#include <iostream>
#include <memory>

using namespace std;

namespace Slam
{
ExecutorImpl::ExecutorImpl()
{
}

/// implementation of Executor::startCalculation
bool
ExecutorImpl::startCalculation(CalculationListener& listener)
{
    // TODO
    listener.calculationFinished(v_currentPosition, true);

    return true;
}

} // close namepsace Slam
