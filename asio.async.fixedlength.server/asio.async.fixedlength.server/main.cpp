//
//  main.cpp
//  asio.async.fixedlength.server
//
//  Created by Liangchuan Gu on 4/13/17.
//  Copyright © 2017 Liangchuan Gu. All rights reserved.
//

#include "ControllerAsyncServer.h"

namespace {
const static unsigned int DEFAULT_THREAD_POOL_SIZE = 10;
const static unsigned int SERVER_PORT = 3333;
} // End of anonymous namespace

int main()
{
    try
    {
        
        ROBOX::data_transfer::Server srv;
        
        unsigned int thread_pool_size = std::thread::hardware_concurrency() * 2;
        
        if (thread_pool_size == 0)
            thread_pool_size = DEFAULT_THREAD_POOL_SIZE;
        
        srv.Start(SERVER_PORT, thread_pool_size);
        
        do
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
        } while (getchar() != 'q');
        
        srv.Stop();
    }
    catch (boost::system::system_error &e)
    {
        std::cout << "Error occured! Error code = " << e.code()
        << ". Message: " << e.what();
    }
}

