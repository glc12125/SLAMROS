#include "ExampleSlamProcessEngine.h"

#include <memory>
#include <iostream>


namespace Slam
{

ExampleSlamProcessEngine::ExampleSlamProcessEngine ():
    m_currentTimestamp(0)
{
}

    
/// implementation of DataReader::queueRequest
bool 
ExampleSlamProcessEngine::queueRequest(DataReader::NewDataListener& listener)
{
    std::cout << "ExampleSlamProcessEngine::queueRequest\n";


    std::shared_ptr<ExampleSlamWorkerItem> exampleSlamWorkerItem(
        new ExampleSlamWorkerItem(listener));

    if (!queueWorkerItem(exampleSlamWorkerItem)) {
        std::cerr << "Cannot enqueued a Example process request\n";

        return false;
    }

    std::cout << "Successfully enqueued a Example process request" << std::endl;

    return true;

}
    
/// implementation of SlamProcessEngine::startProcessData
bool 
ExampleSlamProcessEngine::startProcessData(DataReader::NewDataListener& listener)
{
    //std::unique_lock<std::mutex> mlock(v_queryTSMutex);

    // TODO, could make use of the mutex or sephamore to prevent racing condition

    //v_queryTSCondition.notify_all();
    
    listener.fisnihedProcessingData(true);

    return true;
}

} // Slam