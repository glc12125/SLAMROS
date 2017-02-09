#include "WorkerItem.h"

namespace MathLib {

void workerThreadMainWrapper(void * threadBase)
{
    WorkerThreadBase * workerThreadBase = static_cast<WorkerThreadBase*>(threadBase);
    
    workerThreadBase->workerThreadMain();
}

}
