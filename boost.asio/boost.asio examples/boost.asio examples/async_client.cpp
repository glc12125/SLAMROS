#include <boost/asio.hpp>

#include <thread>
#include <mutex>
#include <memory>
#include <iostream>
#include <unordered_map>
#include <chrono>
#include <sstream>

using namespace boost;

// function which is called when a request is complete.
typedef void(*Callback) (unsigned long request_id,
const std::string& response,
const system::error_code& ec);

// Class represents a context of a single request.
struct Session {
    Session(asio::io_service& ios,
            const std::string& raw_ip_address,
            unsigned short port_num,
            const std::string& request,
            unsigned long id,
            Callback callback) :
    m_sock(ios),
    m_ep(asio::ip::address::from_string(raw_ip_address),
         port_num),
    m_request(request),
    m_id(id),
    m_callback(callback),
    m_was_cancelled(false) {}
    
    asio::ip::tcp::socket m_sock; // Socket used for communication
    asio::ip::tcp::endpoint m_ep; // Remote endpoint.
    std::string m_request;        // Request string for now, will be changed to image buffer
    
    // streambuf where the response will be stored.
    asio::streambuf m_response_buf;
    std::string m_response; // Response represented as a string.
    
    // Contains the description of an error if one occurs during
    // the request lifecycle.
    system::error_code m_ec;
    
    unsigned long m_id; // Unique ID assigned to the request.
    
    // Pointer to the function to be called when the request
    // completes.
    Callback m_callback;
    
    bool m_was_cancelled;
    std::mutex m_cancel_guard;
};

class AsyncTCPClient : public boost::noncopyable {
public:
    AsyncTCPClient(){
        m_work.reset(new boost::asio::io_service::work(m_ios));
        
        m_thread.reset(new std::thread([this](){
            m_ios.run();
        }));
    }
    
    void emulateLongComputationOp(
                                  unsigned int duration_sec,
                                  const std::string& raw_ip_address,
                                  unsigned short port_num,
                                  Callback callback,
                                  unsigned long request_id) {
        
        // Preparing the request string. This will be changed to image buffer
        unsigned long ms = std::chrono::system_clock::now().time_since_epoch()/std::chrono::milliseconds(1);
        std::stringstream ss;
        ss << ms << "@"; // This is a hard-code separator (dirty hack) for now, unless we have a proper serializer that supports a data structure in the buffer. Will reseach
        std::cout << "Timestamp in micro second: " << ss.str() << std::endl;
        std::string request(768000, 'a'); //320 * 240 * 10
        request += "\n" // This is a hard-code separator (dirty hack) for now, unless we have a proper serializer that supports a data structure in the buffer. Will reseach
        + std::to_string(duration_sec)
        + "The_Cool_Termination_Mark"; // This is not a dity hack, it looks like the limitation of asio. There has to be a read-until terminator
        ss << request;
        
        std::shared_ptr<Session> session =
        std::shared_ptr<Session>(new Session(m_ios,
                                             raw_ip_address,
                                             port_num,
                                             ss.str(),
                                             request_id,
                                             callback));
        
        session->m_sock.open(session->m_ep.protocol());
        
        // This addition of sessions will be guarded because we might need to
        // cancel a current session. e.g. during program shutdown.
        // So there should be no data corruption.
        std::unique_lock<std::mutex>
        lock(m_active_sessions_guard);
        m_active_sessions[request_id] = session;
        lock.unlock();
        
        session->m_sock.async_connect(session->m_ep,
                                      [this, session](const system::error_code& ec)
                                      {
                                          if (ec != 0) {
                                              session->m_ec = ec;
                                              onRequestComplete(session);
                                              return;
                                          }
                                          
                                          std::unique_lock<std::mutex>
                                          cancel_lock(session->m_cancel_guard);
                                          
                                          if (session->m_was_cancelled) {
                                              onRequestComplete(session);
                                              return;
                                          }
                                          
                                          asio::async_write(session->m_sock,
                                                            asio::buffer(session->m_request),
                                                            [this, session](const boost::system::error_code& ec,
                                                                            std::size_t bytes_transferred)
                                                            {
                                                                if (ec != 0) {
                                                                    session->m_ec = ec;
                                                                    onRequestComplete(session);
                                                                    return;
                                                                }
                                                                
                                                                std::unique_lock<std::mutex>
                                                                cancel_lock(session->m_cancel_guard);
                                                                
                                                                if (session->m_was_cancelled) {
                                                                    onRequestComplete(session);
                                                                    return;
                                                                }
                                                                
                                                                asio::async_read_until(session->m_sock,
                                                                                       session->m_response_buf,
                                                                                       '\n',
                                                                                       [this, session](const boost::system::error_code& ec,
                                                                                                       std::size_t bytes_transferred)
                                                                                       {
                                                                                           if (ec != 0) {
                                                                                               session->m_ec = ec;
                                                                                           } else {
                                                                                               std::istream strm(&session->m_response_buf);
                                                                                               std::getline(strm, session->m_response);
                                                                                           }
                                                                                           
                                                                                           onRequestComplete(session);
                                                                                       });});});
    };
    
    // Cancels the request.
    void cancelRequest(unsigned int request_id) {
        std::cout << "request with id(" << request_id << ") has been requested to cancel!\n";
        std::unique_lock<std::mutex>
        lock(m_active_sessions_guard);
        
        auto it = m_active_sessions.find(request_id);
        if (it != m_active_sessions.end()) {
            std::unique_lock<std::mutex>
            cancel_lock(it->second->m_cancel_guard);
            
            it->second->m_was_cancelled = true;
            it->second->m_sock.cancel();
            std::cout << "request with id(" << request_id << ") has been found and cancelled!\n";
        }
    }
    
    void close() {
        // Destroy work object. Butter way of doing this?
        m_work.reset(NULL);
        
        // Wait for the I/O thread to exit. Very important!
        // Otherwise there will be uncollected threads, which will
        // eventually exaust system resource.
        m_thread->join();
    }
    
private:
    void onRequestComplete(std::shared_ptr<Session> session) {
        // We do not cared about error code for now.
        boost::system::error_code ignored_ec;
        
        session->m_sock.shutdown(
                                 asio::ip::tcp::socket::shutdown_both,
                                 ignored_ec);
        
        // Again, guard when removing session form the map of active sessions.
        std::unique_lock<std::mutex>
        lock(m_active_sessions_guard);
        
        auto it = m_active_sessions.find(session->m_id);
        if (it != m_active_sessions.end())
            m_active_sessions.erase(it);
        
        lock.unlock();
        
        boost::system::error_code ec;
        
        if (session->m_ec == 0 && session->m_was_cancelled)
            ec = asio::error::operation_aborted;
        else
            ec = session->m_ec;
        
        // Notify via callback
        session->m_callback(session->m_id,
                            session->m_response, ec);
    };
    
private:
    asio::io_service m_ios;
    std::unordered_map<unsigned long, std::shared_ptr<Session>> m_active_sessions;
    std::mutex m_active_sessions_guard;
    std::unique_ptr<boost::asio::io_service::work> m_work;
    std::unique_ptr<std::thread> m_thread;
};

long failureCounts = 0;

void handler(unsigned long request_id,
             const std::string& response, 
             const system::error_code& ec) 
{
    if (ec == 0) {
        std::cout << "Request #" << request_id
        << " has completed. Response: "
        << response << std::endl;
    } else if (ec == asio::error::operation_aborted) {
        std::cout << "Request #" << request_id
        << " has been cancelled by the user." 
        << std::endl;
        ++failureCounts;
    } else {
        std::cout << "Request #" << request_id
        << " failed! Error code = " << ec.value()
        << ". Error message = " << ec.message() 
        << std::endl;
        ++failureCounts;
    }
    
    return;
}

int main()
{
    try {
        AsyncTCPClient client;
        
        /*
         // ID 1, 10 seconds of processing time(bot done yet)
         client.emulateLongComputationOp(10, "127.0.0.1", 3333,
         handler, 1);
         
         std::this_thread::sleep_for(std::chrono::seconds(5));
         // ID2, 11 seconds
         client.emulateLongComputationOp(11, "127.0.0.1", 3333,
         handler, 2);
         // Then decides to cancel the request with id 1.
         client.cancelRequest(1);
         
         std::this_thread::sleep_for(std::chrono::seconds(6));
         // ID 3
         client.emulateLongComputationOp(12, "127.0.0.1", 3333,
         handler, 3);
         
         std::this_thread::sleep_for(std::chrono::seconds(15));
         */
        int frequency = 50;
        long counter = 10*60*1000/frequency;
        while (counter > 3) {
            client.emulateLongComputationOp(10, "127.0.0.1", 3333,
                                            handler, counter);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000/frequency));
            --counter;
        }
        
        std::cout << "Success rate: " << failureCounts/counter * 100 << "%" <<std::endl;
        client.close();
    }
    catch (system::system_error &e) {
        std::cout << "Error occured! Error code = " << e.code()
        << ". Message: " << e.what();
        
        return e.code().value();
    }
    
    return 0;
};
