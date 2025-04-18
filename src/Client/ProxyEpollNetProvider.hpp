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

#include <sys/eventfd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <assert.h>
#include <chrono>
#include <errno.h>
#include <cstring>
#include <string>
#include <string_view>

#include "ProxyConnection.hpp"

template<class BUFFER, class NetProvider>
class ProxyConnector;

template<class BUFFER, class Stream>
class ProxyEpollNetProvider {
public:
	using Buffer_t = BUFFER;
	using Stream_t = Stream;
	using NetProvider_t = ProxyEpollNetProvider<BUFFER, Stream>;
	using Conn_t = ProxyConnection<BUFFER, NetProvider_t >;
	using Connector_t = ProxyConnector<BUFFER, NetProvider_t >;
	ProxyEpollNetProvider(Connector_t &connector);
	~ProxyEpollNetProvider();
	int connect(Conn_t &conn, const ConnectOptions &opts, int client_fd);
	void close(Stream_t &strm);
	void close(Conn_t &conn);
	/** Proxy to sockets; polling using epoll. */
	int WaitClientToTnt();
	int WaitTntToClient();
	int Wait();

	/** <socket : connection> map. Contains both ready to read/send connections */
	int m_ClientEpollFd;
	int m_TntEpollFd;
	int m_AllEpollFd;

private:
	static constexpr int TIMEOUT_INFINITY = -1;
	static constexpr size_t EPOLL_EVENTS_MAX = 128;

	//return 0 if all data from buffer was processed (sent or read);
	//return -1 in case of errors;
	//return 1 in case socket is blocked.
	int sendToClient(Conn_t &conn);
	int sendToTnt(Conn_t &conn);

	int recvFromClient(Conn_t &conn);
	int recvFromTnt(Conn_t &conn);

	void registerEpoll(Conn_t &conn);
	void addToEpoll(Conn_t &conn, int fd);
	void deleteConnectionFromEpoll(Conn_t &conn);
	void deleteStreamFromEpoll(int fd);

	int server_fd_;
	Connector_t &m_Connector;
	std::list<ProxyConnection<BUFFER, ProxyEpollNetProvider>> conns_;
};

template<class BUFFER, class Stream>
ProxyEpollNetProvider<BUFFER, Stream>::ProxyEpollNetProvider(Connector_t &connector) :
	m_Connector(connector)
{
	m_ClientEpollFd = epoll_create1(EPOLL_CLOEXEC);
		if (m_ClientEpollFd == -1) {
		LOG_ERROR("Failed to initialize client epoll: ", strerror(errno));
		abort();
	}
	m_TntEpollFd = epoll_create1(EPOLL_CLOEXEC);
	if (m_TntEpollFd == -1) {
		LOG_ERROR("Failed to initialize tnt epoll: ", strerror(errno));
		abort();
	}
	m_AllEpollFd = epoll_create1(EPOLL_CLOEXEC);
		if (m_ClientEpollFd == -1) {
		LOG_ERROR("Failed to initialize client epoll: ", strerror(errno));
		abort();
	}

	auto &conn = m_Connector.wakeUpConnection_;
	auto &client_strm = conn.get_client_strm();
	client_strm.set_fd(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC));

	auto &tnt_strm = conn.get_tnt_strm();
	tnt_strm.set_fd(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC));
	
	struct epoll_event wake_event;
	wake_event.events = EPOLLIN;
	wake_event.data.ptr = conn.getImpl();
	if (epoll_ctl(m_ClientEpollFd, EPOLL_CTL_ADD, client_strm.get_fd(), &wake_event) != 0) {
		LOG_INFO("HZ");
	}
	if (epoll_ctl(m_TntEpollFd, EPOLL_CTL_ADD, tnt_strm.get_fd(), &wake_event) != 0) {
		LOG_INFO("HZ");
	}
}

template<class BUFFER, class Stream>
ProxyEpollNetProvider<BUFFER, Stream>::~ProxyEpollNetProvider()
{
	::close(m_ClientEpollFd);
	::close(m_TntEpollFd);
	::close(m_AllEpollFd);
	m_ClientEpollFd = -1;
	m_TntEpollFd = -1;
	m_AllEpollFd = -1;
}

template<class BUFFER, class Stream>
void
ProxyEpollNetProvider<BUFFER, Stream>::registerEpoll(Conn_t &conn)
{
	/* Configure epoll with new socket. */
	assert(m_ClientEpollFd >= 0);
	assert(m_TntEpollFd >= 0);
	assert(m_AllEpollFd >= 0);
	struct epoll_event event;
	event.events = EPOLLIN;
	event.data.ptr = conn.getImpl();
	if (epoll_ctl(m_ClientEpollFd, EPOLL_CTL_ADD, conn.get_client_strm().get_fd(),
		      &event) != 0) {
		LOG_ERROR("Failed to add socket to epoll: "
			  "epoll_ctl() returned with errno: ",
			  strerror(errno));
		abort();
	}

	if (epoll_ctl(m_TntEpollFd, EPOLL_CTL_ADD, conn.get_tnt_strm().get_fd(),
		      &event) != 0) {
		LOG_ERROR("Failed to add socket to epoll: "
			  "epoll_ctl() returned with errno: ",
			  strerror(errno));
		abort();
	}

	if (epoll_ctl(m_AllEpollFd, EPOLL_CTL_ADD, conn.get_tnt_strm().get_fd(),
		      &event) != 0) {
		LOG_ERROR("Failed to add socket to epoll: "
			  "epoll_ctl() returned with errno: ",
			  strerror(errno));
		abort();
	}
}

template<class BUFFER, class Stream>
void
ProxyEpollNetProvider<BUFFER, Stream>::addToEpoll(Conn_t &conn, int fd)
{
	assert(m_AllEpollFd >= 0);
	struct epoll_event event;
	event.events = EPOLLIN;
	event.data.fd = fd;
	event.data.ptr = conn.getImpl();

	if (epoll_ctl(m_AllEpollFd, EPOLL_CTL_ADD, fd,
		      &event) != 0) {
		LOG_ERROR("Failed to add socket to epoll: "
			  "epoll_ctl() returned with errno: ",
			  strerror(errno));
		abort();
	}
}

template<class BUFFER, class Stream>
void
ProxyEpollNetProvider<BUFFER, Stream>::deleteConnectionFromEpoll(Conn_t &conn)
{
	assert(m_AllEpollFd >= 0);

	// if (epoll_ctl(m_AllEpollFd, EPOLL_CTL_DEL, fd, nullptr) != 0) {
	// 	LOG_ERROR("Failed to delete socket from epoll: "
	// 		  "epoll_ctl() returned with errno: ",
	// 		  strerror(errno));
	// 	abort();
	// }
}

template<class BUFFER, class Stream>
void
ProxyEpollNetProvider<BUFFER, Stream>::deleteStreamFromEpoll(int fd)
{
	assert(m_AllEpollFd >= 0);

	if (epoll_ctl(m_AllEpollFd, EPOLL_CTL_DEL, fd, nullptr) != 0) {
		LOG_ERROR("Failed to delete socket from epoll: "
			  "epoll_ctl() returned with errno: ",
			  strerror(errno));
		abort();
	}
}

template<class BUFFER, class Stream>
int
ProxyEpollNetProvider<BUFFER, Stream>::connect(Conn_t &conn,
					  const ConnectOptions &opts, int client_fd)
{
	auto &client_strm = conn.get_client_strm();
	client_strm.set_fd(client_fd);

	LOG_DEBUG("Accepted connection: socket is ", client_strm.get_fd());

	auto &tnt_stream = conn.get_tnt_strm();
	if (tnt_stream.connect(opts) < 0) {
		conn.setError("Failed to establish connection to " +
			      opts.address);
		return -1;
	}
	LOG_DEBUG("Connected to ", opts.address, ", socket is ", tnt_stream.get_fd());

	registerEpoll(conn);
	return 0;
}

template<class BUFFER, class Stream>
void
ProxyEpollNetProvider<BUFFER, Stream>::close(Stream_t& strm)
{
	int was_fd = strm.get_fd();
	assert(was_fd >= 0);
	strm.close();
#ifndef NDEBUG
	struct sockaddr sa;
	socklen_t sa_len = sizeof(sa);
	if (getsockname(was_fd, &sa, &sa_len) != -1) {
		char addr[120];
		if (sa.sa_family == AF_INET) {
			struct sockaddr_in *sa_in = (struct sockaddr_in *) &sa;
			snprintf(addr, 120, "%s:%d", inet_ntoa(sa_in->sin_addr),
				 ntohs(sa_in->sin_port));
		} else {
			struct sockaddr_un *sa_un = (struct sockaddr_un *) &sa;
			snprintf(addr, 120, "%s", sa_un->sun_path);
		}
		LOG_DEBUG("Closed connection to socket ", was_fd,
			  " corresponding to address ", addr);
	}
#endif
	epoll_ctl(m_ClientEpollFd, EPOLL_CTL_DEL, was_fd, nullptr); // ?
	epoll_ctl(m_TntEpollFd, EPOLL_CTL_DEL, was_fd, nullptr);
	epoll_ctl(m_AllEpollFd, EPOLL_CTL_DEL, was_fd, nullptr);
}

template<class BUFFER, class Stream>
void
ProxyEpollNetProvider<BUFFER, Stream>::close(Conn_t &conn)
{
	close(conn.get_client_strm());
	close(conn.get_tnt_strm());
	auto it = std::find(m_Connector.conns_.begin(), m_Connector.conns_.begin(), conn);
    
    if (it != m_Connector.conns_.end()) {
        m_Connector.conns_.erase(it);
    }
}

template<class BUFFER, class Stream>
int
ProxyEpollNetProvider<BUFFER, Stream>::recvFromClient(Conn_t &conn)
{
	auto &buf = conn.getClientToTntBuf();
	auto itr = buf.template end<true>();
	buf.write({CONN_READAHEAD});
	struct iovec iov[IOVEC_MAX_SIZE];
	size_t iov_cnt = buf.getIOV(itr, iov, IOVEC_MAX_SIZE);

	ssize_t rcvd = conn.get_client_strm().recv(iov, iov_cnt);
	clientHasNotRecvBytes(conn, CONN_READAHEAD - (rcvd < 0 ? 0 : rcvd));
	if (rcvd < 0) {
		// peer shudown
		return -1;
	}
	LOG_DEBUG("Client->Tnt: Successfully read ", std::to_string(rcvd), " bytes.");
	return rcvd;
}

template<class BUFFER, class Stream>
int
ProxyEpollNetProvider<BUFFER, Stream>::recvFromTnt(Conn_t &conn)
{
	auto &buf = conn.getTntToClientBuf();
	auto itr = buf.template end<true>();
	buf.write({CONN_READAHEAD});
	struct iovec iov[IOVEC_MAX_SIZE];
	size_t iov_cnt = buf.getIOV(itr, iov, IOVEC_MAX_SIZE);

	ssize_t rcvd = conn.get_tnt_strm().recv(iov, iov_cnt);
	tntHasNotRecvBytes(conn, CONN_READAHEAD - (rcvd < 0 ? 0 : rcvd));
	if (rcvd < 0) {
		conn.setError(std::string("Failed to receive response: ") +
					  strerror(errno), errno);
		return -1;
	}
	LOG_DEBUG("Tnt->Client: Successfully read ", std::to_string(rcvd), " bytes.");
	return rcvd;
}

template<class BUFFER, class Stream>
int
ProxyEpollNetProvider<BUFFER, Stream>::sendToClient(Conn_t &conn)
{
	while (tntHasDataToSend(conn)) {
		struct iovec iov[IOVEC_MAX_SIZE];
		auto &buf = conn.getTntToClientBuf();
		size_t iov_cnt = buf.getIOV(buf.template begin<true>(),
					    iov, IOVEC_MAX_SIZE);

		ssize_t sent = conn.get_client_strm().send(iov, iov_cnt);
		if (sent < 0) {
			conn.setError(std::string("Failed to send request: ") +
				      strerror(errno), errno);
			return -1;
		} else if (sent == 0) {
			return 1;
		} else {
			tntHasSentBytes(conn, sent);
			LOG_DEBUG("Tnt->Client: Successfully send ", sent, " bytes.");
		}
	}
	/* All data from connection has been successfully written. */
	return 0;
}

template<class BUFFER, class Stream>
int
ProxyEpollNetProvider<BUFFER, Stream>::sendToTnt(Conn_t &conn)
{
	while (clientHasDataToSend(conn)) {
		struct iovec iov[IOVEC_MAX_SIZE];
		auto &buf = conn.getClientToTntBuf();
		size_t iov_cnt = buf.getIOV(buf.template begin<true>(),
					    iov, IOVEC_MAX_SIZE);

		ssize_t sent = conn.get_tnt_strm().send(iov, iov_cnt);
		if (sent < 0) {
			conn.setError(std::string("Failed to send request: ") +
				      strerror(errno), errno);
			return -1;
		} else if (sent == 0) {
			return 1;
		} else {
			clientHasSentBytes(conn, sent);
			LOG_DEBUG("Client->Tnt: Successfully send ", sent, " bytes.");
		}
	}
	return 0;
}

template<class BUFFER, class Stream>
int
ProxyEpollNetProvider<BUFFER, Stream>::WaitClientToTnt()
{
	struct epoll_event events[EPOLL_EVENTS_MAX];
	int event_cnt = epoll_wait(m_ClientEpollFd, events, EPOLL_EVENTS_MAX, TIMEOUT_INFINITY);
	if (event_cnt < 0) {
		LOG_ERROR("Poll failed: ", strerror(errno));
		return -1;
	}
	for (int i = 0; i < event_cnt; ++i) {
		Conn_t conn((typename Conn_t::Impl_t *)events[i].data.ptr);
		if (conn == m_Connector.wakeUpConnection_) {
			uint64_t wake_up;
			if (!read(conn.get_client_strm().get_fd(), &wake_up, sizeof(uint64_t))) {
				LOG_DEBUG("SAD");
			}
			return 0;
		}
		if ((events[i].events & EPOLLIN) != 0) {
			LOG_DEBUG("Client->Tnt: EPOLLIN - socket ",
				  conn.get_client_strm().get_fd(),
				  " is ready to read");

			int rc = recvFromClient(conn);
			if (rc < 0) {
				// peer shutdown
				close(conn);
				return -1;
			}
			assert(clientHasDataToDecode(conn));

			if (connectionDecodeRequests(conn) == -1) {
				close(conn);
				return -1;
			}

			sendToTnt(conn);
		}
	}
	return 0;
}

template<class BUFFER, class Stream>
int
ProxyEpollNetProvider<BUFFER, Stream>::WaitTntToClient()
{
	struct epoll_event events[EPOLL_EVENTS_MAX];
	int event_cnt = epoll_wait(m_TntEpollFd, events, EPOLL_EVENTS_MAX, TIMEOUT_INFINITY);
	if (event_cnt < 0) {
		LOG_ERROR("Poll failed: ", strerror(errno));
		return -1;
	}
	for (int i = 0; i < event_cnt; ++i) {
		Conn_t conn((typename Conn_t::Impl_t *)events[i].data.ptr);
		if (conn == m_Connector.wakeUpConnection_) {
			uint64_t wake_up;
			read(conn.get_tnt_strm().get_fd(), &wake_up, sizeof(uint64_t));
			return 0;
		}
		if ((events[i].events & EPOLLIN) != 0) {
			LOG_DEBUG("Tnt->Client: EPOLLIN - socket ",
				  conn.get_tnt_strm().get_fd(),
				  " is ready to read");

			int rc = recvFromTnt(conn);
			if (rc < 0) {
				close(conn);
				return -1;
			}
			sendToClient(conn);
		}
	}
	return 0;
}

template<class BUFFER, class Stream>
int
ProxyEpollNetProvider<BUFFER, Stream>::Wait()
{
	struct epoll_event events[EPOLL_EVENTS_MAX];
	int event_cnt = epoll_wait(m_AllEpollFd, events, EPOLL_EVENTS_MAX, TIMEOUT_INFINITY);
	if (event_cnt < 0) {
		LOG_ERROR("Poll failed: ", strerror(errno));
		return -1;
	}
	for (int i = 0; i < event_cnt; ++i) {
		int current_fd = events[i].data.fd;
		if (current_fd == server_fd_) {
            sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd_, 
                                 reinterpret_cast<sockaddr*>(&client_addr),
                                 &client_len);
            if (client_fd == -1) {
				LOG_ERROR("Accept failed", strerror(errno));
				// stop();
				// return;
            }
			conns_.emplace_back(ProxyConnection<BUFFER, ProxyEpollNetProvider>(m_Connector));
			conns_.back().get_client_strm().set_fd(client_fd);
			addToEpoll(conns_.back(), client_fd);
		} else {
			Conn_t conn((typename Conn_t::Impl_t *)events[i].data.ptr);
			if ((events[i].events & EPOLLIN) != 0) {
				if (current_fd == conn.getImpl().get_client_strm()) {
					// Client Request
					int rc = recvFromClient(conn);
					if (rc < 0) {
						close(conn);
						return -1;
				}
				} else {
					// External service (e.g Tarantool) response
					int rc = recvFromTnt(conn);
					if (rc < 0) {
						close(conn);
						return -1;
					}
				}
				// send bytes to Callback

				sendToClient(conn);
			}
		}
	}
	return 0;
}
