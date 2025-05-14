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
#include <set>

#include "ProxyConnection.hpp"

template<class BUFFER, class NetProvider>
class ProxyConnector;

template<class BUFFER, class Stream>
class ProxyEpollNetProvider {
public:
	using Buffer_t = BUFFER;
	using Stream_t = Stream;
	using NetProvider_t = ProxyEpollNetProvider<BUFFER, Stream>;
	using Conn_t = ProxyConnection<BUFFER, NetProvider_t>;
	using Connector_t = ProxyConnector<BUFFER, NetProvider_t >;
	ProxyEpollNetProvider(Connector_t &connector);
	~ProxyEpollNetProvider();
	void close(Stream_t &strm);
	void close(Conn_t &conn);
	/** Proxy to sockets; polling using epoll. */
	void Wait();
	void SetServerFd(int fd);
	void AcceptNewClient();

	int m_AllEpollFd;

	//return 0 if all data from buffer was processed (sent or read);
	//return -1 in case of errors;
	//return 1 in case socket is blocked.
	Stream_t& connect(Conn_t &conn, int instance_index);
	int sendDec(Conn_t &conn, Stream_t &strm, int size = -1);
	int sendEnc(Conn_t &conn, Stream_t &strm, int size = -1);
	int recv(Conn_t &conn, Stream_t &strm);

	void addToEpoll(Conn_t *conn, Stream_t &strm);

	Connector_t &m_Connector;

	std::map<Stream_t *, Conn_t*> strm_to_conn;
	std::set<int> greeting_expected_on_fd;
	std::set<int> new_clients;
	std::map<int, int> instance_id_to_active_connetions;

private:
	Stream_t server_strm_; 
	static constexpr int TIMEOUT_INFINITY = -1;
	static constexpr size_t EPOLL_EVENTS_MAX = 128;

	std::list<ProxyConnection<BUFFER, ProxyEpollNetProvider>> conns_;
};

template<class BUFFER, class Stream>
ProxyEpollNetProvider<BUFFER, Stream>::ProxyEpollNetProvider(Connector_t &connector) :
	m_Connector(connector)
{
	m_AllEpollFd = epoll_create1(EPOLL_CLOEXEC);
		if (m_AllEpollFd == -1) {
		LOG_ERROR("Failed to initialize client epoll: ", strerror(errno));
		abort();
	}
}

template<class BUFFER, class Stream>
ProxyEpollNetProvider<BUFFER, Stream>::~ProxyEpollNetProvider()
{
	::close(m_AllEpollFd);
	m_AllEpollFd = -1;
}

template<class BUFFER, class Stream>
void
ProxyEpollNetProvider<BUFFER, Stream>::addToEpoll(Conn_t *conn, Stream_t &strm)
{
	assert(m_AllEpollFd >= 0);
	struct epoll_event event;
	event.events = EPOLLIN;
	event.data.ptr = &strm;
	strm_to_conn[&strm] = conn;

	if (epoll_ctl(m_AllEpollFd, EPOLL_CTL_ADD, strm.get_fd(),
		      &event) != 0) {
		LOG_ERROR("Failed to add socket to epoll: "
			  "epoll_ctl() returned with errno: ",
			  strerror(errno));
		abort();
	}
}

template<class BUFFER, class Stream>
void
ProxyEpollNetProvider<BUFFER, Stream>::SetServerFd(int fd)
{
	server_strm_.set_fd(fd);
	addToEpoll(nullptr, server_strm_);
}

template<class BUFFER, class Stream>
void
ProxyEpollNetProvider<BUFFER, Stream>::AcceptNewClient()
{
	// Accept
	sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	int client_fd = accept(server_strm_.get_fd(), 
							reinterpret_cast<sockaddr*>(&client_addr),
							&client_len);
	if (client_fd == -1) {
		LOG_ERROR("Accept failed", strerror(errno));
		// stop();
		return;
	}

	new_clients.insert(client_fd);
	// Add to Epoll
	conns_.emplace_back(ProxyConnection<BUFFER, ProxyEpollNetProvider>(m_Connector));
	conns_.back().get_client_strm().set_fd(client_fd);
	addToEpoll(&conns_.back(), conns_.back().get_client_strm());
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
	epoll_ctl(m_AllEpollFd, EPOLL_CTL_DEL, was_fd, nullptr);
}

template<class BUFFER, class Stream>
void
ProxyEpollNetProvider<BUFFER, Stream>::close(Conn_t &conn)
{
	// stop streams
	close(conn.get_client_strm());
	for (auto &c : conn.get_external_strms()) {
		instance_id_to_active_connetions[c.first]--;

		strm_to_conn.erase(&c.second);
		greeting_expected_on_fd.erase(c.second.get_fd());
		close(c.second);
	}

	// remove from vector of connections
	auto it = std::find(conns_.begin(), conns_.end(), conn);
    
    if (it != conns_.end()) {
        conns_.erase(it);
    }
}

template<class BUFFER, class Stream>
int
ProxyEpollNetProvider<BUFFER, Stream>::recv(Conn_t &conn, Stream_t &strm)
{
	// prepare buff
	auto& buf = conn.getDecBuf();
	auto itr = buf.template end<true>();
	buf.write({CONN_READAHEAD});
	struct iovec iov[IOVEC_MAX_SIZE];
	size_t iov_cnt = buf.getIOV(itr, iov, IOVEC_MAX_SIZE);
	
	// recv
	ssize_t rcvd = strm.recv(iov, iov_cnt);
	hasNotRecvBytes(conn, CONN_READAHEAD - (rcvd < 0 ? 0 : rcvd));
	if (rcvd < 0) {
		// peer shudown
		return -1;
	}
	return rcvd;
}

template<class BUFFER, class Stream>
int
ProxyEpollNetProvider<BUFFER, Stream>::sendDec(Conn_t &conn, Stream_t &strm, int size)
{
	if (hasDecodedDataToSend(conn)) {
		// prepare buff
		struct iovec iov[IOVEC_MAX_SIZE];
		auto& buf = conn.getDecBuf();

		size_t iov_cnt;
		if (size != -1) {
			auto end_of_req = buf.template begin<true>() + size;
			iov_cnt = buf.getIOV(buf.template begin<true>(), end_of_req,
					    iov, IOVEC_MAX_SIZE);
		} else {
			iov_cnt = buf.getIOV(buf.template begin<true>(),
				iov, IOVEC_MAX_SIZE);
		}

		// send
		ssize_t sent = strm.send(iov, iov_cnt);
		if (sent < 0) {
			conn.setError(std::string("Failed to send request: ") +
				      strerror(errno), errno);
			return -1;
		} else if (sent == 0) {
			return 1;
		} else {
			hasSentDecodedData(conn, sent);
		}
	}
	/* All data from connection has been successfully written. */
	return 0;
}

template<class BUFFER, class Stream>
int
ProxyEpollNetProvider<BUFFER, Stream>::sendEnc(Conn_t &conn, Stream_t &strm, int size)
{
	if (hasEncodedDataToSend(conn)) {
		// prepare buff
		struct iovec iov[IOVEC_MAX_SIZE];
		auto& buf = conn.getEncBuf();

		size_t iov_cnt;
		if (size != -1) {
			auto end_of_req = buf.template begin<true>() + size;
			iov_cnt = buf.getIOV(buf.template begin<true>(), end_of_req,
					    iov, IOVEC_MAX_SIZE);
		} else {
			iov_cnt = buf.getIOV(buf.template begin<true>(),
				iov, IOVEC_MAX_SIZE);
		}

		// send
		ssize_t sent = strm.send(iov, iov_cnt);
		if (sent < 0) {
			conn.setError(std::string("Failed to send request: ") +
				      strerror(errno), errno);
			return -1;
		} else if (sent == 0) {
			return 1;
		} else {
			hasSentEncodedData(conn, sent);
		}
	}
	/* All data from connection has been successfully written. */
	return 0;
}

template<class BUFFER, class Stream>
Stream&
ProxyEpollNetProvider<BUFFER, Stream>::connect(Conn_t &conn, int instance_index)
{
	auto &strm = conn.getImpl()->instance_index_to_strm[instance_index];
	if (strm.get_fd() > 0) return strm;
	strm.connect(m_Connector.opts_[instance_index]);
	if (m_Connector.opts_[instance_index].is_tnt) {
		greeting_expected_on_fd.insert(strm.get_fd());
	}
	instance_id_to_active_connetions[instance_index]++;
	addToEpoll(&conn, strm);
	return strm;
}

template<class BUFFER, class Stream>
void
ProxyEpollNetProvider<BUFFER, Stream>::Wait()
{
	struct epoll_event events[EPOLL_EVENTS_MAX];
	int event_cnt = epoll_wait(m_AllEpollFd, events, EPOLL_EVENTS_MAX, TIMEOUT_INFINITY);
	if (event_cnt < 0) {
		LOG_ERROR("Poll failed: ", strerror(errno));
		std::abort();
	}
	for (int i = 0; i < event_cnt; ++i) {
		Stream_t *current_strm = (Stream_t *)events[i].data.ptr;
		if (current_strm->get_fd() == server_strm_.get_fd()) {
			AcceptNewClient();
		} else {
			Conn_t *conn = strm_to_conn[current_strm];
			if ((events[i].events & EPOLLIN) != 0) {
				int rc = recv(*conn, *current_strm);
				if (rc < 0) {
					close(*conn);
					return;
				}


				m_Connector.setCurrentReceiver(conn, current_strm);
				// send bytes to Callback
				m_Connector.customHandler();
				
				auto it = new_clients.find(current_strm->get_fd());
				if (it != new_clients.end()) {
					new_clients.erase(current_strm->get_fd());
				}
			}
		}
	}
}
