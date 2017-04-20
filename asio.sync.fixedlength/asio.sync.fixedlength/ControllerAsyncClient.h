//
//  FixLengthAsyncClient.hpp
//  asio.sync.fixedlength
//
//  Created by lgu41 on 4/19/17.
//  Copyright © 2017 Liangchuan Gu. All rights reserved.
//

#ifndef INCLUDED_FIXLENGTH_ASYNC_CLIENT_H
#define INCLUDED_FIXLENGTH_ASYNC_CLIENT_H

#include <boost/asio.hpp>

#include <thread>
#include <mutex>
#include <memory>
#include <unordered_map>


namespace ROBOX
{
namespace data_transfer
{

struct Session;
    
// function which is called when a request is complete.
typedef void (*Callback)(unsigned long request_id, const std::string &response,
                         const boost::system::error_code &ec);
    
class ControllerAsyncClient : public boost::noncopyable {
public:
    ControllerAsyncClient();
    ~ControllerAsyncClient();
    
    void startControllerTCPClient(const std::string& cmd,
                        const std::string& raw_ip_address,
                        unsigned short port_num,
                        Callback callback,
                        unsigned long request_id);
    
    // Cancels the request.
    void cancelRequest(unsigned int request_id);
    
    void close();
    
private:
    
    void onRequestComplete(std::shared_ptr<Session> session);
    
private:
    boost::asio::io_service d_ios;
    std::unordered_map<unsigned long, std::shared_ptr<Session>> d_active_sessions;
    std::mutex d_active_sessions_guard;
    std::unique_ptr<boost::asio::io_service::work> d_work;
    std::unique_ptr<std::thread> d_thread;
};
    
    
} // End of namespace data_transfer
} // End of namespace ROBOX

#endif /* INCLUDED_FIXLENGTH_ASYNC_CLIENT_H */
