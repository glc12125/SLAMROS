#include <boost/asio.hpp>

#include <thread>
#include <mutex>
#include <memory>
#include <iostream>
#include <unordered_map>
#include <chrono>
#include <sstream>

using namespace boost;

class Session;

// function which is called when a request is complete.
typedef void(*Callback) (const boost::system::error_code& ec,
std::size_t bytes_transferred,
std::shared_ptr<Session> s);

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
    m_content(request),
    m_total_bytes_written(0),
    m_id(id),
    m_callback(callback),
    m_was_cancelled(false) {}
    
    asio::ip::tcp::socket m_sock; // Socket used for communication
    asio::ip::tcp::endpoint m_ep; // Remote endpoint.
    std::string m_content;        // Request string for now, will be changed to image buffer
    std::size_t m_total_bytes_written;
    
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
        
        session->m_sock.async_write_some(
                                         asio::buffer(session->m_content),
                                         std::bind(callback,
                                                   std::placeholders::_1,
                                                   std::placeholders::_2,
                                                   session));
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


void handler(const boost::system::error_code& ec,
             std::size_t bytes_transferred,
             std::shared_ptr<Session> s)
{
    if (ec == asio::error::operation_aborted) {
        std::cout << "Request #" << s->m_id
        << " has been cancelled by the user."
        << std::endl;
        ++failureCounts;
        return ;
    } else if (ec != 0){
        std::cout << "Request #" << s->m_id
        << " failed! Error code = " << ec.value()
        << ". Error message = " << ec.message()
        << std::endl;
        ++failureCounts;
        return;
    }
    
    
    s->m_total_bytes_written += bytes_transferred;
    
    if (s->m_total_bytes_written == s->m_content.length()) {
        std::cout << "Request #" << s->m_id
        << " has completed. " << std::endl;
        return;
    }
    
    s->m_sock.async_write_some(
                               asio::buffer(
                                            s->m_content.c_str() +
                                            s->m_total_bytes_written,
                                            s->m_content.length() -
                                            s->m_total_bytes_written),
                               std::bind(handler, std::placeholders::_1,
                                         std::placeholders::_2, s));
}

int main()
{
    try {
        AsyncTCPClient client;
        
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
