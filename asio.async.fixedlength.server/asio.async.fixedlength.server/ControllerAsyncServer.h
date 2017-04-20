//
//  ControllerAsyncServer.hpp
//  asio.async.fixedlength.server
//
//  Created by lgu41 on 4/20/17.
//  Copyright © 2017 Liangchuan Gu. All rights reserved.
//

#ifndef INCLUDED_FIXLENGTH_ASYNC_SERVER_H
#define INCLUDED_FIXLENGTH_ASYNC_SERVER_H

#include <boost/asio.hpp>

#include <atomic>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <map>
#include <memory>
#include <thread>

namespace ROBOX
{
namespace data_transfer
{
// Keeps objects we need in a callback to
// identify whether all data has been read
// from the socket and to initiate next async
// reading operation if needed.
struct Session;

// Represent each process of image stream, will be changed to SLAM algorithm in
// ProcessRequest
class Service
{
public:
    class ServiceCallback
    {
    public:
        virtual void executeCmd(bool cmdParsed, const std::string& cmd = "") = 0;
    };
    
public:
    Service(std::shared_ptr<boost::asio::ip::tcp::socket> sock,
            ServiceCallback& serviceCallback);
    
    void StartHandling();
    
private:
    // @note:   Function used as a callback for
    //          asynchronous reading operation.
    //          Checks if all data has been read
    //          from the socket and initiates
    //          new readnig operation if needed.
    void onAsyncCallResume(const boost::system::error_code& ec,
                           std::size_t bytes_transferred,
                           std::shared_ptr<Session> s);
    
    void onCmdReceived(const std::string& cmd);
    
    void onResponseSent(const boost::system::error_code &ec,
                        std::size_t bytes_transferred);
    
    
    std::string processCmd(const std::string& cmd);
    
    void onFinish();
    
private:
    std::shared_ptr<boost::asio::ip::tcp::socket> d_sock;
    ServiceCallback &d_serviceCallback;
};

// Each server has exactly one Acceptor that manages connections.
class Acceptor : public Service::ServiceCallback
{
public:
    Acceptor(boost::asio::io_service &ios, unsigned short port_num);
    
    void Start();
    
    void Stop();
    
    // Implement the interface of Service::ServiceCallback::executeCmd
    virtual void executeCmd(bool cmdParsed, const std::string& cmd = "");
    
private:
    void InitAccept();
    
    void onAccept(const boost::system::error_code &ec,
                  std::shared_ptr<boost::asio::ip::tcp::socket> sock);
    
private:
    boost::asio::io_service &d_ios;
    boost::asio::ip::tcp::acceptor d_acceptor;
    std::atomic<bool> d_isStopped;
    boost::shared_mutex d_mutex;
};

class Server
{
public:
    Server();
    
    void Start(unsigned short port_num, unsigned int thread_pool_size);
    
    void Stop();
    
private:
    boost::asio::io_service d_ios;
    std::unique_ptr<boost::asio::io_service::work> d_work;
    std::unique_ptr<Acceptor> d_acceptor;
    std::vector<std::unique_ptr<std::thread>> d_thread_pool;
};

} // End of namespace data_transfer
} // End of namespace ROBOX

#endif /* INCLUDED_FIXLENGTH_ASYNC_SERVER_H */
