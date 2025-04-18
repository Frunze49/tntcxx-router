#pragma once

#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <string>

#include "Connector.hpp"
#include "../Buffer/Buffer.hpp"
#include "arpa/inet.h"

using Buf_t = tnt::Buffer<16 * 1024>;
using Net_t = EpollNetProvider<Buf_t, DefaultStream>;


class UnixProxy : public UnixStream {
public:
    UnixProxy(const std::string& listen_addr, uint16_t listen_port,
              const std::string& target_addr, uint16_t target_port)
        : listen_addr_(listen_addr), listen_port_(listen_port),
          target_addr_(target_addr), target_port_(target_port) {}
    
    ~UnixProxy() { stop(); }

    UnixProxy(const UnixProxy&) = delete;
    UnixProxy& operator=(const UnixProxy&) = delete;
    UnixProxy(UnixProxy&&) = delete;
    UnixProxy& operator=(UnixProxy&&) = delete;

    /**
     * Start the proxy server.
     * Returns 0 on success, -1 on error.
     */
    int start();
    
    /**
     * Stop the proxy server and close all connections.
     */
    void stop();

private:
    void accept_connections();
    void handle_client(std::unique_ptr<UnixPlainStream> client_stream);
    void proxy_data(UnixPlainStream* from,
                    UnixPlainStream* to,
                    const std::string& comment = "");

    std::string listen_addr_;
    uint16_t listen_port_;
    std::string target_addr_;
    uint16_t target_port_;
    
    std::atomic<bool> running_{false};
    std::vector<std::thread> worker_threads_;

    int server_fd;
};

/////////////////////////////////////////////////////////////////////
////////////////////////// Implementation  //////////////////////////
/////////////////////////////////////////////////////////////////////

inline int
UnixProxy::start()
{
    if (running_) {
        return US_DIE("Proxy is already running");
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        return US_DIE("Socket creation failed", strerror(errno));
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        stop();
        return US_DIE("Setsockopt failed", strerror(errno));
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(listen_addr_.c_str());
    addr.sin_port = htons(listen_port_);

    if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        stop();
        return US_DIE("Bind failed", strerror(errno));
    }

    if (listen(server_fd, 128) < 0) {
        stop();
        return US_DIE("Listen failed", strerror(errno));
    }

    running_ = true;
    worker_threads_.emplace_back(&UnixProxy::accept_connections, this);

    return 0;
}

inline void
UnixProxy::stop()
{
    if (!running_) return;
    
    running_ = false;
    
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
}

inline void
UnixProxy::accept_connections()
{
    while (running_) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd, 
                             reinterpret_cast<sockaddr*>(&client_addr),
                             &client_len);
        if (client_fd < 0) {
            if (!running_) break;
            if (errno != EINTR) {
                US_DIE("Accept failed", strerror(errno));
            }
            continue;
        }

        auto client_stream = std::make_unique<UnixPlainStream>();
        client_stream->set_fd(client_fd);

        worker_threads_.emplace_back(
            &UnixProxy::handle_client, this, std::move(client_stream));
    }
}

inline void
UnixProxy::handle_client(std::unique_ptr<UnixPlainStream> client_stream)
{
    auto target_stream = std::make_unique<UnixPlainStream>();
    
    ConnectOptions opts;
    opts.transport = STREAM_PLAIN;
    opts.address = target_addr_;
    opts.service = std::to_string(target_port_);
    
    if (target_stream->connect(opts) < 0) {
        US_DIE("Failed to connect to target server");
        return;
    }

    auto client_shared = std::shared_ptr<UnixPlainStream>(client_stream.release());
    auto target_shared = std::shared_ptr<UnixPlainStream>(target_stream.release());

    // Используем лямбда-функции для безопасного захвата
    auto proxy_func = [this](auto from, auto to, const std::string& comment) {
        this->proxy_data(from.get(), to.get(), comment);
    };

    // Создаем потоки с shared_ptr
    std::thread client_to_target(proxy_func, client_shared, target_shared, "Client->Target");
    std::thread target_to_client(proxy_func, target_shared, client_shared, "Target->Client");

    // Отсоединяем потоки, чтобы они работали независимо
    client_to_target.detach();
    target_to_client.detach();
}

inline void
UnixProxy::proxy_data(UnixPlainStream* from,
                      UnixPlainStream* to,
                      const std::string& comment)
{
    Buf_t buffer;
    struct iovec iov;
    iov.iov_base = &buffer;
    iov.iov_len = buffer.blockSize();

    while (running_) {
        ssize_t bytes_read = from->recv(&iov, 1);
        if (bytes_read > 0) {
            iov.iov_len = bytes_read;
            ssize_t bytes_sent = to->send(&iov, 1);
            std::cout << "Bytes read: " << bytes_read << std::endl;
            std::cout << "Bytes sent: " << bytes_sent << std::endl;
            if (bytes_sent != bytes_read) {
                US_DIE("Failed to send all data");
                break;
            }
        } else if (bytes_read == 0) {
            continue;
        } else if (bytes_read == -1) {
            if (from->has_status(SS_NEED_READ_EVENT_FOR_READ)) {
                continue;
            }
            LOG_DEBUG("Connection", comment, " is dead");
            break;
        }
    }
    from->shutdown();
    to->shutdown();
}