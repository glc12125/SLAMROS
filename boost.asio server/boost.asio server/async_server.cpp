#include <boost/asio.hpp>

#include <thread>
#include <atomic>
#include <memory>
#include <iostream>
#include <map>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>

using namespace boost;

// Represent each process of image stream, will be changed to SLAM algorithm in ProcessRequest
class Service {
public:
    class ServiceCallback {
    public:
        virtual void newImageReceived(const std::string& key, const std::string& imageData) = 0;
    };
public:
    Service(std::shared_ptr<asio::ip::tcp::socket> sock, ServiceCallback& serviceCallback) :
    m_sock(sock),
    m_serviceCallback(serviceCallback)
    {}
    
    void StartHandling() {
        
        asio::async_read_until(*m_sock.get(),
                               m_request,
                               "The_Cool_Termination_Mark",
                               [this](
                                      const boost::system::error_code& ec,
                                      std::size_t bytes_transferred)
                               {
                                   onRequestReceived(ec,
                                                     bytes_transferred);
                               });
    }
    
private:
    void onRequestReceived(const boost::system::error_code& ec,
                           std::size_t bytes_transferred) {
        if (ec != 0) {
            std::cout << "Error occured! Error code = "
            << ec.value()
            << ". Message: " << ec.message();
            
            onFinish();
            return;
        }
        
        m_response = ProcessRequest(m_request);
        
        asio::async_write(*m_sock.get(),
                          asio::buffer(m_response),
                          [this](
                                 const boost::system::error_code& ec,
                                 std::size_t bytes_transferred)
                          {
                              onResponseSent(ec,
                                             bytes_transferred);
                          });
    }
    
    void onResponseSent(const boost::system::error_code& ec,
                        std::size_t bytes_transferred) {
        std::cout << "response has been sent!\n";
        if (ec != 0) {
            std::cout << "Error occured! Error code = "
            << ec.value()
            << ". Message: " << ec.message();
        }
        
        onFinish();
    }
    
    void onFinish() {
        delete this;
    }
    
    // This is the meat part wich does SLAM processing
    std::string ProcessRequest(asio::streambuf& request) {
        
        std::istream input(&request);
        std::string timeStampKey;
        getline(input, timeStampKey, '@');
        std::cout << timeStampKey << std::endl;
        std::string line;
        getline(input, line, '\n');
        //std::cout << line << std::endl;
        
        // This will add the image data to the ImageContainer ordered by timeStampKey
        m_serviceCallback.newImageReceived(timeStampKey, line);
        
        // Thread blocking algorithms
        /*std::this_thread::sleep_for(
         std::chrono::milliseconds(2000));*/
        
        // Prepare and return the response message.
        std::string response = "Response sent \n";
        return response;
    }
    
private:
    std::shared_ptr<asio::ip::tcp::socket> m_sock;
    std::string m_response;
    asio::streambuf m_request;
    ServiceCallback& m_serviceCallback;
};

// Each server has exactly one Acceptor that manages connections.
class Acceptor : public Service::ServiceCallback{
public:
    Acceptor(asio::io_service& ios, unsigned short port_num) :
    m_ios(ios),
    m_acceptor(m_ios,
               asio::ip::tcp::endpoint(
                                       asio::ip::address_v4::any(),
                                       port_num)),
    m_isStopped(false)
    {}
    
    void Start() {
        m_acceptor.listen();
        InitAccept();
    }
    
    void Stop() {
        m_isStopped.store(true);
    }
    
    // Implement the interface of Service::ServiceCallback::newImageReceived
    virtual void newImageReceived(const std::string& key, const std::string& imageData)
    {
        boost::unique_lock<boost::shared_mutex> scoped_lock(m_mutex);
        m_imageContainer[key] = imageData;
        /* 
         * It is verified that the image data is ordered by timestamp since Epoch in milliseconds
        for (auto& t : m_imageContainer) {
            std::cout << "Key: " << t.first << "\n";
        }*/
        // NOTE:: when reading this container, we do not need a unique_lock, we can use a reader lock such as the following:
        /*void reader()
        {
            // get shared access
            boost::shared_lock<boost::shared_mutex> shared_lock(m_mutex);
            
            // now we have shared access
        }
        * Could create more complex data structure as the value of the container, to mark if a image data has been processed, 
        * so that the SLAM algorithm can be multi-threaded.
        */
    }
    
private:
    void InitAccept() {
        std::shared_ptr<asio::ip::tcp::socket>
        sock(new asio::ip::tcp::socket(m_ios));
        
        m_acceptor.async_accept(*sock.get(),
                                [this, sock](
                                             const boost::system::error_code& error)
                                {
                                    onAccept(error, sock);
                                });
    }
    
    void onAccept(const boost::system::error_code& ec,
                  std::shared_ptr<asio::ip::tcp::socket> sock)
    {
        if (ec == 0) {
            // Create a new process for processing image
            (new Service(sock, *this))->StartHandling();
        }
        else {
            std::cout << "Error occured! Error code = "
            << ec.value()
            << ". Message: " << ec.message();
        }
        
        // Init next async accept directly if
        // acceptor has not been stopped yet.
        if (!m_isStopped.load()) {
            InitAccept();
        }
        else {
            m_acceptor.close();
        }
    }
    
private:
    asio::io_service& m_ios;
    asio::ip::tcp::acceptor m_acceptor;
    std::atomic<bool> m_isStopped;
    boost::shared_mutex m_mutex;
    typedef std::map<std::string, std::string> ImageContainer;
    ImageContainer m_imageContainer;
};

class Server {
public:
    Server() {
        m_work.reset(new asio::io_service::work(m_ios));
    }
    
    void Start(unsigned short port_num,
               unsigned int thread_pool_size) {
        
        assert(thread_pool_size > 0);
        
        // Create and start Acceptor.
        acc.reset(new Acceptor(m_ios, port_num));
        acc->Start();
        
        // Each thread simply starts a server side listening.
        for (unsigned int i = 0; i < thread_pool_size; i++) {
            std::unique_ptr<std::thread> th(
                                            new std::thread([this]()
                                                            {
                                                                m_ios.run();
                                                            }));
            
            m_thread_pool.push_back(std::move(th));
        }
    }
    
    void Stop() {
        acc->Stop();
        m_ios.stop();
        
        for (auto& th : m_thread_pool) {
            th->join();
        }
    }
    
private:
    asio::io_service m_ios;
    std::unique_ptr<asio::io_service::work> m_work;
    std::unique_ptr<Acceptor> acc;
    std::vector<std::unique_ptr<std::thread>> m_thread_pool;
};


const unsigned int DEFAULT_THREAD_POOL_SIZE = 10;

int main()
{
    unsigned short port_num = 3333;
    
    try {
        Server srv;
        
        unsigned int thread_pool_size =
        std::thread::hardware_concurrency() * 2;
        
        if (thread_pool_size == 0)
            thread_pool_size = DEFAULT_THREAD_POOL_SIZE;
        
        srv.Start(port_num, thread_pool_size);
        
        std::this_thread::sleep_for(std::chrono::minutes(10));
        
        srv.Stop();
    }
    catch (system::system_error &e) {
        std::cout << "Error occured! Error code = "
        << e.code() << ". Message: "
        << e.what();
    }
    
    return 0;
}
