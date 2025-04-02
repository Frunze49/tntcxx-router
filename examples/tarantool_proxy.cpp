#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

using boost::asio::ip::tcp;

class TarantoolProxy {
public:
    TarantoolProxy(boost::asio::io_service& io_service, 
                  const std::string& tarantool_host, short tarantool_port,
                  short proxy_port)
        : io_service_(io_service),
          tarantool_host_(tarantool_host),
          tarantool_port_(tarantool_port),
          acceptor_(io_service, tcp::endpoint(tcp::v4(), proxy_port)) {
        start_accept();
    }

private:
    void start_accept() {
        auto new_connection = std::make_shared<tcp::socket>(io_service_);
        acceptor_.async_accept(*new_connection,
            [this, new_connection](const boost::system::error_code& error) {
                if (!error) {
                    std::thread(&TarantoolProxy::handle_client, this, new_connection).detach();
                } else {
                    std::cerr << "Accept error: " << error.message() << std::endl;
                }
                start_accept();
            });
    }

    void handle_client(std::shared_ptr<tcp::socket> client_socket) {
        try {
            // Подключаемся к Tarantool
            tcp::resolver resolver(io_service_);
            tcp::resolver::query query(tarantool_host_, std::to_string(tarantool_port_));
            auto endpoint_iterator = resolver.resolve(query);
            
            tcp::socket tarantool_socket(io_service_);
            boost::asio::connect(tarantool_socket, endpoint_iterator);

            // Устанавливаем таймауты
            client_socket->set_option(tcp::socket::reuse_address(true));
            tarantool_socket.set_option(tcp::socket::reuse_address(true));
            
            // Буферы для данных
            std::vector<char> client_to_tarantool(4096);
            std::vector<char> tarantool_to_client(4096);
            
            // Обеспечиваем двунаправленную передачу данных
            std::thread([&]() {
                try {
                    while (true) {
                        size_t bytes_read = client_socket->read_some(
                            boost::asio::buffer(client_to_tarantool));
                        if (bytes_read == 0) break;
                        
                        boost::asio::write(tarantool_socket, 
                            boost::asio::buffer(client_to_tarantool, bytes_read));
                    }
                } catch (std::exception& e) {
                    std::cerr << "Client to Tarantool error: " << e.what() << std::endl;
                }
            }).detach();
            
            // Проксируем ответы от Tarantool клиенту
            try {
                while (true) {
                    size_t bytes_read = tarantool_socket.read_some(
                        boost::asio::buffer(tarantool_to_client));
                    if (bytes_read == 0) break;                
                    
                    boost::asio::write(*client_socket, 
                        boost::asio::buffer(tarantool_to_client, bytes_read));
                }
            } catch (std::exception& e) {
                std::cerr << "Tarantool to client error: " << e.what() << std::endl;
            }
            
            // Закрываем соединения
            client_socket->close();
            tarantool_socket.close();
            
        } catch (std::exception& e) {
            std::cerr << "Connection error: " << e.what() << std::endl;
        }
    }

    boost::asio::io_service& io_service_;
    std::string tarantool_host_;
    short tarantool_port_;
    tcp::acceptor acceptor_;
};

int main() {
    try {
        boost::asio::io_service io_service;
        TarantoolProxy proxy(io_service, "127.0.0.1", 3301, 3302);
        std::cout << "Proxy server started on port 3302" << std::endl;
        
        // Запускаем несколько потоков для обработки соединений
        std::vector<std::thread> threads;
        for (int i = 0; i < 4; ++i) {
            threads.emplace_back([&io_service]() { io_service.run(); });
        }
        
        for (auto& t : threads) {
            t.join();
        }
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}