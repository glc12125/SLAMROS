#ifndef HEADER_INCLUDED_ASYNC_CLIENT
#define HEADER_INCLUDED_ASYNC_CLIENT

#include <boost/asio.hpp>

#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

// TODO: forward declaration

namespace ROBOX {
    
    namespace image_xfer {
        
        // function which is called when a request is complete.
        typedef void (*Callback)(unsigned long request_id, const std::string& response,
        const boost::system::error_code& ec);
        struct Session;
        
        class AsyncTCPClient : public boost::noncopyable {
        public:
            AsyncTCPClient();
            
            void sendImageAsync(const std::string& raw_ip_address,
                                unsigned short port_num, Callback callback,
                                unsigned long request_id, const char* imageData);
            
            // Cancels the request.
            void cancelRequest(unsigned int request_id);
            
            void close();
            
        private:
            void onRequestComplete(std::shared_ptr<Session> session);
            
        private:
            boost::asio::io_service m_ios;
            std::unordered_map<unsigned long, std::shared_ptr<Session>> m_active_sessions;
            std::mutex m_active_sessions_guard;
            std::unique_ptr<boost::asio::io_service::work> m_work;
            std::unique_ptr<std::thread> m_thread;
        };
        
    }  // End of namespace image_xfer
    
}  // End of namespace ROBOX

#endif  // endif HEADER_INCLUDED_ASYNC_CLIENT
