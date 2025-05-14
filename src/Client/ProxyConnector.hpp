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
	ProxyConnector(const std::vector<ConnectOptions>& opts, const std::string &listen_addr, uint16_t &listen_port);
	~ProxyConnector();
	ProxyConnector(const ProxyConnector& connector) = delete;
	ProxyConnector& operator = (const ProxyConnector& connector) = delete;
	//////////////////////////////Main API//////////////////////////////////
	/**
     * Start the proxy server.
     * Returns 0 on success, -1 on error.
     */
    void start();
	void acceptConnections();

	void customHandler();
	void setCurrentReceiver(ProxyConnection<BUFFER, NetProvider> *conn, typename NetProvider::Stream_t *recv_strm);

	std::optional<Message<BUFFER>>
		getNextDecodedMessage();

	typename NetProvider::Stream_t& connect(int instance_index);
	int sendDecodedToStream(typename NetProvider::Stream_t &strm, int size);
	int sendDecodedToClient(int size);
	void skipLastDecodedMessage(int size);

	int sendEncodedToStream(typename NetProvider::Stream_t &strm, int size);
	int sendEncodedToClient(int size);
	void skipLastEncodedMessage(int size);

	bool isClientFirstRequest();
	bool isConnectedToInstance(int intance_id);
	std::vector<int> getConnectedInstances();
	bool isRecvFromClient();
	bool isGreetingExpected();
	int deliverDecodedGreeting();

	int getInstanceConnectionAmount(int instance_id);

	template <size_t N>
	int deliverEncodedGreeting(char (&buf)[N]);

	int createMessage(int sync, int schema_id);
	template <class T>
	int createMessage(int sync, int schema_id, const T *data = nullptr);
	
	std::vector<ConnectOptions> opts_;

private:
	NetProvider m_NetProvider_;
	ProxyConnection<BUFFER, NetProvider> *current_conn;
	typename NetProvider::Stream_t *current_strm;

	std::string listen_addr_;
	uint16_t listen_port_;
	static constexpr int MAX_OPEN_CONNECTIONS = 128;
};

template<class BUFFER, class NetProvider>
ProxyConnector<BUFFER, NetProvider>::ProxyConnector(const std::vector<ConnectOptions>& opts,
												    const std::string &listen_addr,
													uint16_t &listen_port) :
										opts_(opts), m_NetProvider_(*this), 
								 		listen_addr_(listen_addr), listen_port_(listen_port)
{
}

template<class BUFFER, class NetProvider>
ProxyConnector<BUFFER, NetProvider>::~ProxyConnector()
{
}

template<class BUFFER, class NetProvider>
void ProxyConnector<BUFFER, NetProvider>::start()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
		LOG_ERROR("Socket creation failed", strerror(errno));
        return;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		LOG_ERROR("Setsockopt failed", strerror(errno));
        return;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(listen_addr_.c_str());
    addr.sin_port = htons(listen_port_);

    if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
		LOG_ERROR("Bind failed", strerror(errno));
        return;
    }

    if (listen(server_fd, 128) < 0) {
		LOG_ERROR("Listen failed", strerror(errno));
        return;
    }
	m_NetProvider_.SetServerFd(server_fd);
    acceptConnections();
}

template<class BUFFER, class NetProvider>
void ProxyConnector<BUFFER, NetProvider>::acceptConnections()
{
	LOG_INFO("Server started on: ", listen_addr_, ":", listen_port_);
    while (true) {
		m_NetProvider_.Wait();
    }
}

template<class BUFFER, class NetProvider>
void ProxyConnector<BUFFER, NetProvider>::setCurrentReceiver(ProxyConnection<BUFFER, NetProvider> *conn, typename NetProvider::Stream_t *recv_strm)
{
	current_conn = conn;
	current_strm = recv_strm;
}

template<class BUFFER, class NetProvider>
std::optional<Message<BUFFER>>
ProxyConnector<BUFFER, NetProvider>::getNextDecodedMessage()
{
	if (hasDataToDecode(*current_conn)) {
        Message<BUFFER> message;
		DecodeStatus rc = processMessage(*current_conn, &message);
		if (rc == DECODE_ERR)
			return std::nullopt;

		if (rc == DECODE_NEEDMORE)
			return std::nullopt;
		assert(rc == DECODE_SUCC);

		return message;
	}
	return std::nullopt;
}

template<class BUFFER, class NetProvider>
int
ProxyConnector<BUFFER, NetProvider>::sendDecodedToStream(typename NetProvider::Stream_t &strm, int size)
{
	assert(strm.get_fd() > 0);
	return m_NetProvider_.sendDec(*current_conn, strm, size);
}

template<class BUFFER, class NetProvider>
int
ProxyConnector<BUFFER, NetProvider>::sendDecodedToClient(int size)
{
	return sendDecodedToStream(current_conn->get_client_strm(), size);
}

template<class BUFFER, class NetProvider>
void
ProxyConnector<BUFFER, NetProvider>::skipLastDecodedMessage(int size)
{
	hasSentEncodedData(*current_conn, size);
}

template<class BUFFER, class NetProvider>
int
ProxyConnector<BUFFER, NetProvider>::sendEncodedToStream(typename NetProvider::Stream_t &strm, int size)
{
	assert(strm.get_fd() > 0);
	return m_NetProvider_.sendEnc(*current_conn, strm, size);
}

template<class BUFFER, class NetProvider>
int
ProxyConnector<BUFFER, NetProvider>::sendEncodedToClient(int size)
{
	return sendEncodedToStream(current_conn->get_client_strm(), size);
}


template<class BUFFER, class NetProvider>
typename NetProvider::Stream_t&
ProxyConnector<BUFFER, NetProvider>::connect(int instance_index)
{
	assert(instance_index >= 0);
	auto it = current_conn->getImpl()->instance_index_to_strm.find(instance_index);
	bool is_new_strm = it == current_conn->getImpl()->instance_index_to_strm.end();
	if (is_new_strm) {
		auto &strm = m_NetProvider_.connect(*current_conn, instance_index);
		return strm;
	}
	return it->second;
}

template<class BUFFER, class NetProvider>
bool
ProxyConnector<BUFFER, NetProvider>::isRecvFromClient()
{
	return current_conn->get_client_strm().get_fd() == current_strm->get_fd();
}

template<class BUFFER, class NetProvider>
bool
ProxyConnector<BUFFER, NetProvider>::isClientFirstRequest()
{
	if (current_strm->get_fd() != current_conn->get_client_strm().get_fd()) return false;
	return m_NetProvider_.new_clients.find(current_strm->get_fd()) != m_NetProvider_.new_clients.end();
}

template<class BUFFER, class NetProvider>
bool
ProxyConnector<BUFFER, NetProvider>::isConnectedToInstance(int instance_id)
{
	return current_conn->is_connected_to_instance(instance_id);
}

template<class BUFFER, class NetProvider>
std::vector<int>
ProxyConnector<BUFFER, NetProvider>::getConnectedInstances()
{
	return current_conn->get_connected_instances();
}

template<class BUFFER, class NetProvider>
bool
ProxyConnector<BUFFER, NetProvider>::isGreetingExpected()
{
	if (isRecvFromClient()) return false;

	auto it = m_NetProvider_.greeting_expected_on_fd.find(current_strm->get_fd());
	return it != m_NetProvider_.greeting_expected_on_fd.end();
}

template<class BUFFER, class NetProvider>
template <size_t N>
int
ProxyConnector<BUFFER, NetProvider>::deliverEncodedGreeting(char (&greeting_buf)[N])
{
	assert(!hasEncodedDataToSend(*current_conn));
	current_conn->getImpl()->encBuffer.write(greeting_buf);
	return m_NetProvider_.sendEnc(*current_conn, current_conn->get_client_strm());
}

template<class BUFFER, class NetProvider>
int
ProxyConnector<BUFFER, NetProvider>::deliverDecodedGreeting()
{
	auto it = m_NetProvider_.greeting_expected_on_fd.find(current_strm->get_fd());
	m_NetProvider_.greeting_expected_on_fd.erase(it);

	current_conn->getImpl()->endDecoded += Iproto::GREETING_SIZE;
    current_conn->getImpl()->dec.reset(current_conn->getImpl()->endDecoded);
	return m_NetProvider_.sendDec(*current_conn, current_conn->get_client_strm(), Iproto::GREETING_SIZE);
}

template<class BUFFER, class NetProvider>
int
ProxyConnector<BUFFER, NetProvider>::getInstanceConnectionAmount(int instance_id)
{
	return m_NetProvider_.instance_id_to_active_connetions[instance_id];
}

template<class BUFFER, class NetProvider>
template <class T>
int
ProxyConnector<BUFFER, NetProvider>::createMessage(int sync, int schema_id, const T *data)
{
	return current_conn->getImpl()->enc.encodeOk(sync, schema_id, data);
}

template<class BUFFER, class NetProvider>
int
ProxyConnector<BUFFER, NetProvider>::createMessage(int sync, int schema_id)
{
	return current_conn->getImpl()->enc.encodeOk(sync, schema_id);
}