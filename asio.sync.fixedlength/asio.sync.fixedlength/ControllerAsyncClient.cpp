//
//  FixLengthAsyncClient.cpp
//  asio.sync.fixedlength
//
//  Created by lgu41 on 4/19/17.
//  Copyright © 2017 Liangchuan Gu. All rights reserved.
//

#include "ControllerAsyncClient.h"

#include <sstream>
#include <iostream>

namespace ROBOX
{
namespace data_transfer
{

// Class represents a context of a single request.
struct Session {
    Session(boost::asio::io_service& ios,
            const std::string& raw_ip_address,
            unsigned short port_num,
            const std::string& request,
            unsigned long id,
            Callback callback) :
    d_sock(ios),
    d_ep(boost::asio::ip::address::from_string(raw_ip_address),
         port_num),
    d_content(request),
    d_total_bytes_written(0),
    d_id(id),
    d_callback(callback),
    d_was_cancelled(false) {}
    
    boost::asio::ip::tcp::socket d_sock; // Socket used for communication
    boost::asio::ip::tcp::endpoint d_ep; // Remote endpoint.
    std::string d_content;        // Request string for now, will be changed to image buffer
    std::size_t d_total_bytes_written;
    
    // streambuf where the response will be stored.
    boost::asio::streambuf d_response_buf;
    std::string d_response; // Response represented as a string.
    
    // Contains the description of an error if one occurs during
    // the request lifecycle.
    boost::system::error_code d_ec;
    
    unsigned long d_id; // Unique ID assigned to the request.
    
    // Pointer to the function to be called when the request
    // completes.
    Callback d_callback;
    
    bool d_was_cancelled;
    std::mutex d_cancel_guard;
};

    
ControllerAsyncClient::ControllerAsyncClient() {
    d_work.reset(new boost::asio::io_service::work(d_ios));
    d_thread.reset(new std::thread([this](){
        d_ios.run();
    }));
}

ControllerAsyncClient::~ControllerAsyncClient()
{
    close();
}

void ControllerAsyncClient::startControllerTCPClient(const std::string& cmd,
                    const std::string& raw_ip_address,
                    unsigned short port_num,
                    Callback callback,
                    unsigned long request_id) {
    
    
    std::shared_ptr<Session> session =
    std::shared_ptr<Session>(new Session(d_ios,
                                         raw_ip_address,
                                         port_num,
                                         cmd,
                                         request_id,
                                         callback));
    
    session->d_sock.open(session->d_ep.protocol());
    
    // Add new session to the list of active sessions so
    // that we can access it if the user decides to cancel
    // the corresponding request before it completes.
    // Because active sessions list can be accessed from
    // multiple threads, we guard it with a mutex to avoid
    // data corruption.
    std::unique_lock<std::mutex>
    lock(d_active_sessions_guard);
    d_active_sessions[request_id] = session;
    lock.unlock();
    
    session->d_sock.async_connect(session->d_ep,
                                  [this, session](const boost::system::error_code& ec)
    {
        if (ec != 0) {
            session->d_ec = ec;
            onRequestComplete(session);
            return;
        }
        
        std::unique_lock<std::mutex>
        cancel_lock(session->d_cancel_guard);
        
        if (session->d_was_cancelled) {
            onRequestComplete(session);
            return;
        }
        
        boost::asio::async_write(session->d_sock,
                          boost::asio::buffer(session->d_content),
                          [this, session](const boost::system::error_code& ec,
                                          std::size_t bytes_transferred)
        {
            if (ec != 0) {
                session->d_ec = ec;
                onRequestComplete(session);
                return;
            }
            
            std::unique_lock<std::mutex>
            cancel_lock(session->d_cancel_guard);
            
            if (session->d_was_cancelled) {
                onRequestComplete(session);
                return;
            }
            
            boost::asio::async_read_until(session->d_sock,
                                   session->d_response_buf,
                                   '\n', 
                                   [this, session](const boost::system::error_code& ec,
                                                   std::size_t bytes_transferred) 
            {
                if (ec != 0) {
                    session->d_ec = ec;
                } else {
                    std::istream strm(&session->d_response_buf);
                    std::getline(strm, session->d_response);
                }
                
                onRequestComplete(session);
            });});});
}

// Cancels the request.
void ControllerAsyncClient::cancelRequest(unsigned int request_id) {
    std::cout << "request with id(" << request_id << ") has been requested to cancel!\n";
    std::unique_lock<std::mutex>
    lock(d_active_sessions_guard);
    
    auto it = d_active_sessions.find(request_id);
    if (it != d_active_sessions.end()) {
        std::unique_lock<std::mutex>
        cancel_lock(it->second->d_cancel_guard);
        
        it->second->d_was_cancelled = true;
        it->second->d_sock.cancel();
        std::cout << "request with id(" << request_id << ") has been found and cancelled!\n";
    }
}

void ControllerAsyncClient::close() {
    // Destroy work object. Butter way of doing this?
    d_work.reset(NULL);
    
    // Wait for the I/O thread to exit. Very important!
    // Otherwise there will be uncollected threads, which will
    // eventually exaust system resource.
    if (d_thread->joinable())
    {
       d_thread->join(); 
    }
}


void ControllerAsyncClient::onRequestComplete(std::shared_ptr<Session> session) {
    // We do not cared about error code for now.
    boost::system::error_code ignored_ec;
    
    session->d_sock.shutdown(
                             boost::asio::ip::tcp::socket::shutdown_both,
                             ignored_ec);
    
    // Again, guard when removing session form the map of active sessions.
    std::unique_lock<std::mutex>
    lock(d_active_sessions_guard);
    
    auto it = d_active_sessions.find(session->d_id);
    if (it != d_active_sessions.end())
        d_active_sessions.erase(it);
    
    lock.unlock();
    
    boost::system::error_code ec;
    
    if (session->d_ec == 0 && session->d_was_cancelled)
        ec = boost::asio::error::operation_aborted;
    else
        ec = session->d_ec;
    
    // Notify via callback
    session->d_callback(session->d_id,
                        session->d_response, ec);
};
    
} // End of namespace data_transfer
} // End of namespace ROBOX
