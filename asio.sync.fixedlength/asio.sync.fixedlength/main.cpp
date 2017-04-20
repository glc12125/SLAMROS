#include <iostream>

#include "ControllerAsyncClient.h"

namespace {
    
static unsigned long s_failureCounts = 0;
    
} // End of anonymous namespace

void handler(unsigned long request_id, const std::string &response,
             const boost::system::error_code &ec)
{
    if (ec == 0)
    {
        std::cout << "Request #" << request_id
        << " has completed. Response: " << response << std::endl;
    }
    else if (ec == boost::asio::error::operation_aborted)
    {
        std::cout << "Request #" << request_id << " has been cancelled by the user."
        << std::endl;
        ++s_failureCounts;
    }
    else
    {
        std::cout << "Request #" << request_id
        << " failed! Error code = " << ec.value()
        << ". Error message = " << ec.message() << std::endl;
        ++s_failureCounts;
    }
    
    return;
}

namespace
{
    static const unsigned int MESSAGE_SIZE = 7;
}

std::string padCmd(const std::string& cmd)
{
    std::string result(cmd);
    result.append(MESSAGE_SIZE - cmd.size(), ' ');
    
    return result;
}

int main()
{
    try {
        ROBOX::data_transfer::ControllerAsyncClient client;
        
        int frequency = 50;
        long counter = 10*60*100/frequency;
        long total = counter;
        std::vector<std::string> MockCmd = {"UP", "RIGHT", "DOWN", "LEFT"};
        while (counter > 3) {
            client.startControllerTCPClient(padCmd(MockCmd[counter%4]), "127.0.0.1", 3333, handler, counter);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000/frequency));
            --counter;
        }
        
        std::cout << "Success rate: " << (total - s_failureCounts)/total * 100 << "%" <<std::endl;
        client.close();
    }
    catch (boost::system::system_error &e) {
        std::cout << "Error occured! Error code = " << e.code()
        << ". Message: " << e.what();
        
        return e.code().value();
    }
    
    return 0;
};
