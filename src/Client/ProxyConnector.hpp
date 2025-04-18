#pragma once
/*
 * Copyright 2010-2020, Tarantool AUTHORS, please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <thread>
#include <atomic>
#include <mutex>
#include <list>

#include "ProxyConnection.hpp"

#ifdef TNTCXX_ENABLE_SSL
#include "UnixSSLStream.hpp"
#else
#include "UnixPlainStream.hpp"
#endif

#include "../Utils/Timer.hpp"

#include <set>

#ifdef TNTCXX_ENABLE_SSL
using DefaultStream = UnixSSLStream;
#else
using DefaultStream = UnixPlainStream;
#endif

/**
 * MacOS does not have epoll so let's use Libev as default network provider.
 */
#ifdef __linux__
#include "ProxyEpollNetProvider.hpp"
template<class BUFFER>
using DefaultNetProvider = ProxyEpollNetProvider<BUFFER, DefaultStream>;
#else
#include "LibevNetProvider.hpp"
template<class BUFFER>
using DefaultNetProvider = LibevNetProvider<BUFFER, DefaultStream>;
#endif

template<class BUFFER, class NetProvider = DefaultNetProvider<BUFFER>>
class ProxyConnector
{
public:
	using RequestHandler = std::function<void(Request<BUFFER>)>;

	ProxyConnector(const ConnectOptions& opts, const std::string &listen_addr, uint16_t &listen_port,
				   RequestHandler request_handler);
	~ProxyConnector();
	ProxyConnector(const ProxyConnector& connector) = delete;
	ProxyConnector& operator = (const ProxyConnector& connector) = delete;
	//////////////////////////////Main API//////////////////////////////////
	/**
     * Start the proxy server.
     * Returns 0 on success, -1 on error.
     */
    int start();
    
    /**
     * Stop the proxy server and close all connections.
     */
    void stop();
	void deleteOneConnection(bool isClientToTnt);
	void connect(ProxyConnection<BUFFER, NetProvider> &conn,
		    const ConnectOptions &opts, int client_fd);
	void acceptConnections();

	void waitFromClient();
	void waitFromTnt();

	void processRequest(Request<BUFFER>&& request);

	ProxyConnection<BUFFER, NetProvider> wakeUpConnection_;
	std::list<ProxyConnection<BUFFER, NetProvider>> conns_;

private:
	
	int server_fd_;
	
	std::atomic<int> open_connections_{0};
	std::atomic<bool> running_{false};
	std::atomic<bool> waiting_{false};

	std::atomic<bool> new_client_{true};
	std::atomic<bool> new_tnt_{true};

	std::mutex mtx_;
	std::thread acceptor_thread_, client_thread_, tnt_thread_;	

	NetProvider m_NetProvider_;

	ConnectOptions opts_;
	std::string listen_addr_;
	uint16_t listen_port_;

	RequestHandler request_handler_;

	static constexpr int MAX_OPEN_CONNECTIONS = 128;
};

template<class BUFFER, class NetProvider>
ProxyConnector<BUFFER, NetProvider>::ProxyConnector(const ConnectOptions& opts,
												    const std::string &listen_addr,
													uint16_t &listen_port,
													RequestHandler request_handler) :
										wakeUpConnection_(*this), m_NetProvider_(*this),
										opts_(opts), listen_addr_(listen_addr), listen_port_(listen_port),
										request_handler_(std::move(request_handler))
{
	(void)wakeUpConnection_;
}

template<class BUFFER, class NetProvider>
ProxyConnector<BUFFER, NetProvider>::~ProxyConnector()
{
}

template<class BUFFER, class NetProvider>
int ProxyConnector<BUFFER, NetProvider>::start()
{
    if (running_) {
		LOG_ERROR("Proxy is already running");
        return 1;
    }

    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
		LOG_ERROR("Socket creation failed", strerror(errno));
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        stop();
		LOG_ERROR("Setsockopt failed", strerror(errno));
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(listen_addr_.c_str());
    addr.sin_port = htons(listen_port_);

    if (bind(server_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        stop();
		LOG_ERROR("Bind failed", strerror(errno));
        return 1;
    }

    if (listen(server_fd_, 128) < 0) {
        stop();
		LOG_ERROR("Listen failed", strerror(errno));
        return 1;
    }

    running_ = true;
    acceptor_thread_ = std::thread(&ProxyConnector::acceptConnections, this);
	acceptor_thread_.detach();

    return 0;
}

template<class BUFFER, class NetProvider>
void ProxyConnector<BUFFER, NetProvider>::stop()
{
	std::lock_guard<std::mutex> lock(mtx_);
    if (!running_) return;

    running_ = false;
	waiting_ = false;
	open_connections_ = 0;

	m_NetProvider_.close(wakeUpConnection_);
	close(server_fd_);
}

template<class BUFFER, class NetProvider>
void ProxyConnector<BUFFER, NetProvider>::deleteOneConnection(bool isClientToTnt)
{
	std::lock_guard<std::mutex> lock(mtx_);
	if (open_connections_ < 1) return;

	--open_connections_;
	if (open_connections_ == 0) {
		LOG_INFO("Stop polling threads");
		waiting_ = false;
		
		uint64_t wake_up = 1;
		if (isClientToTnt) {
			write(wakeUpConnection_.get_tnt_strm().get_fd(), &wake_up, sizeof(uint64_t));
		} else {
			write(wakeUpConnection_.get_client_strm().get_fd(), &wake_up, sizeof(uint64_t));
		}
	}
}

template<class BUFFER, class NetProvider>
void
ProxyConnector<BUFFER, NetProvider>::connect(ProxyConnection<BUFFER, NetProvider> &conn,
					const ConnectOptions &opts, int client_fd)
{
	//Make sure that connection is not yet established.
	assert(conn.get_client_strm().has_status(SS_DEAD));
	assert(conn.get_tnt_strm().has_status(SS_DEAD));
	if (m_NetProvider_.connect(conn, opts, client_fd) != 0) {
		LOG_ERROR("Failed to connect to ",
			  opts.address, ':', opts.service);
		std::abort();
	}

	{
		std::lock_guard<std::mutex> lock(mtx_);
		open_connections_++;
		waiting_ = true;
		assert(open_connections_ < MAX_OPEN_CONNECTIONS);
	}

	LOG_DEBUG("Connection to ", opts.address, ':', opts.service,
		  " has been established");
}

template<class BUFFER, class NetProvider>
void ProxyConnector<BUFFER, NetProvider>::acceptConnections()
{
    while (running_) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd_, 
                             reinterpret_cast<sockaddr*>(&client_addr),
                             &client_len);
        if (client_fd < 0) {
			LOG_ERROR("Accept failed", strerror(errno));
            stop();
			return;
        }
		conns_.emplace_back(ProxyConnection<BUFFER, NetProvider>(*this));
		connect(conns_.back(), opts_, client_fd);
		
		assert(new_client_ == new_tnt_);
		if (new_client_ && new_tnt_) {
			client_thread_ = std::thread(&ProxyConnector::waitFromClient, this);
			tnt_thread_ = std::thread(&ProxyConnector::waitFromTnt, this);

			client_thread_.detach();
			tnt_thread_.detach();
		}
    }
}

template<class BUFFER, class NetProvider>
void
ProxyConnector<BUFFER, NetProvider>::waitFromClient()
{
	LOG_INFO("Started client receiver");
	new_client_ = false;
	while (waiting_)
	{
		if (m_NetProvider_.WaitClientToTnt() != 0)
			deleteOneConnection(true);
	}
	new_client_ = true;
	// del conn
}

template<class BUFFER, class NetProvider>
void
ProxyConnector<BUFFER, NetProvider>::waitFromTnt()
{
	LOG_INFO("Started tarantool receiver");
	new_tnt_ = false;
	while (waiting_)
	{
		if (m_NetProvider_.WaitTntToClient() != 0)
			deleteOneConnection(false);
	}
	new_tnt_ = true;
}
template<class BUFFER, class NetProvider>
void
ProxyConnector<BUFFER, NetProvider>::processRequest(Request<BUFFER>&& request) {
	request_handler_(std::move(request));
}
