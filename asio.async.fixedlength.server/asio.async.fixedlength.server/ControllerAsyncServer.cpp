//
//  ControllerAsyncServer.cpp
//  asio.async.fixedlength.server
//
//  Created by lgu41 on 4/20/17.
//  Copyright © 2017 Liangchuan Gu. All rights reserved.
//

#include "ControllerAsyncServer.h"

#include <boost/asio.hpp>

#include <atomic>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cerrno>

namespace ROBOX{
namespace data_transfer
{

namespace
{
    static const unsigned int MESSAGE_SIZE = 7;
}

// Keeps objects we need in a callback to
// identify whether all data has been read
// from the socket and to initiate next async
// reading operation if needed.
struct Session {
    std::shared_ptr<boost::asio::ip::tcp::socket> sock;
    std::unique_ptr<char[]> buf;
    unsigned int buf_size;
};


Service::Service(std::shared_ptr<boost::asio::ip::tcp::socket> sock,
        ServiceCallback& serviceCallback)
: d_sock(sock), d_serviceCallback(serviceCallback)
{
}

void Service::StartHandling()
{
    std::shared_ptr<Session> s(new Session);
    
    // Step 4. Allocating the buffer.
    s->buf.reset(new char[MESSAGE_SIZE]);
    
    s->sock = d_sock;
    s->buf_size = MESSAGE_SIZE;
    
    // Step 5. Initiating asynchronous reading opration.
    s->sock->async_read_some(
                             boost::asio::buffer(s->buf.get(), s->buf_size),
                             std::bind(&Service::onAsyncCallResume, std::ref(*this),
                                       std::placeholders::_1,
                                       std::placeholders::_2,
                                       s));
    
}
    

void Service::onAsyncCallResume(const boost::system::error_code& ec,
                       std::size_t bytes_transferred,
                       std::shared_ptr<Session> s)
{
    if (ec != 0) {
        std::cout << "Error occured! Error code = "
        << ec.value()
        << ". Message: " << ec.message();
        boost::asio::async_write(*d_sock.get(), boost::asio::buffer("Error reading CMD\n"),
                                 [this](const boost::system::error_code &ec,
                                        std::size_t bytes_transferred)
                                 {
                                     onResponseSent(ec, bytes_transferred);
                                 });
        return;
    }
    
    // Here we know that the reading has completed
    // successfully and the buffer is full with
    // data read from the socket.
    std::string cmd;
    onCmdReceived(cmd.assign(s->buf.get(), s->buf_size));
    
}
    
void Service::onCmdReceived(const std::string& cmd)
{
    
    
    auto response = processCmd(cmd);
    
    boost::asio::async_write(*d_sock.get(), boost::asio::buffer(response),
                             [this](const boost::system::error_code &ec,
                                    std::size_t bytes_transferred)
                             {
                                 onResponseSent(ec, bytes_transferred);
                             });
}
    
void Service::onResponseSent(const boost::system::error_code &ec,
                    std::size_t bytes_transferred)
{
    std::cout << "response sent!\n";
    if (ec != 0)
    {
        std::cout << "Error occured! Error code = " << ec.value()
        << ". Message: " << ec.message();
    }
    
    onFinish();
}

namespace
{
    static const std::vector<std::string> SUPPORTED_CMDS{"UP", "DOWN", "LEFT", "RIGHT"};
    std::string trim(const std::string& s)
    {
        std::string result(s);
        size_t p = result.find_first_not_of(" \t");
        result.erase(0, p);
        
        p = result.find_last_not_of(" \t");
        if (std::string::npos != p)
            result.erase(p+1);
        return result;
    }
}

std::string Service::processCmd(const std::string& cmd)
{
    std::string response;
    std::string trimedCmd(trim(cmd));
    if (std::find(SUPPORTED_CMDS.begin(), SUPPORTED_CMDS.end(), trimedCmd) != SUPPORTED_CMDS.end())
    {
        // Check against a list of supported cmds
        // And then execute the cmd via robot api
        // Thread blocking algorithms
        try {
            d_serviceCallback.executeCmd(true, cmd);
            // Prepare and return the response message.
            response = cmd + " EXECUTED\n";
        } catch (const std::exception& e) {
            std::cerr << "Exception when executing cmd: " << cmd << "\n"
                      << "error: " << e.what() << std::endl;
            response = cmd + " FAILED\n";
        }
        
        std::cout << "Response: " << response << std::endl;
    }
    else
    {
        response = cmd + " NOT SUPPORTED\n";
    }

    return response;
}
    
void Service::onFinish()
{
    delete this;
}
    

Acceptor::Acceptor(boost::asio::io_service &ios, unsigned short port_num)
: d_ios(ios),
d_acceptor(d_ios, boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::any(),
                                                 port_num)),
d_isStopped(false)
{
}

void Acceptor::Start()
{
    std::cout << "asio acceptor started listening\n";
    d_acceptor.listen();
    InitAccept();
}

void Acceptor::Stop()
{
    d_isStopped.store(true);
}

// Implement the interface of Service::ServiceCallback::executeCmd
void Acceptor::executeCmd(bool cmdParsed, const std::string& cmd)
{
    // Emulate sending command to robot/drone
    // Will need to catch exceptions if necessary
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
}
    
    
void Acceptor::InitAccept()
{
    std::shared_ptr<boost::asio::ip::tcp::socket> sock(new boost::asio::ip::tcp::socket(d_ios));
    
    d_acceptor.async_accept(*sock.get(),
                            [this, sock](const boost::system::error_code &error)
                            {
                                std::cout << "Request accepted!\n";
                                onAccept(error, sock);
                            });
}

void Acceptor::onAccept(const boost::system::error_code &ec,
              std::shared_ptr<boost::asio::ip::tcp::socket> sock)
{
    if (ec == 0)
    {
        // Create a new process for processing image
        (new Service(sock, *this))->StartHandling();
    }
    else
    {
        std::cout << "Error occured! Error code = " << ec.value()
        << ". Message: " << ec.message();
    }
    
    // Init next async accept directly if
    // acceptor has not been stopped yet.
    if (!d_isStopped.load())
    {
        InitAccept();
    }
    else
    {
        d_acceptor.close();
    }
}
    

Server::Server()
{
    d_work.reset(new boost::asio::io_service::work(d_ios));
}
    
void Server::Start(unsigned short port_num, unsigned int thread_pool_size)
{
    assert(thread_pool_size > 0);
    
    // Create and start Acceptor.
    d_acceptor.reset(new Acceptor(d_ios, port_num));
    d_acceptor->Start();
    
    // Each thread simply starts a server side listening.
    for (unsigned int i = 0; i < thread_pool_size; i++)
    {
        std::unique_ptr<std::thread> th(new std::thread([this]()
                                                        {
                                                            d_ios.run();
                                                        }));
        
        d_thread_pool.push_back(std::move(th));
    }
}
    
void Server::Stop()
{
    d_acceptor->Stop();
    d_ios.stop();
    
    for (auto &th : d_thread_pool)
    {
        th->join();
    }
}
    
} // End of namespace data_transfer
} // End of namespace ROBOX

