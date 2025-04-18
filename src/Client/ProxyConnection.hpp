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

#include "RequestDecoder.hpp"

#include "Stream.hpp"
#include "../Utils/Logger.hpp"

#include <sys/uio.h> //iovec
#include <string>
#include <unordered_map> //futures

/** rid == request id */
typedef size_t rid_t;

static constexpr size_t CONN_READAHEAD = 64 * 1024;
static constexpr size_t IOVEC_MAX_SIZE = 32;

struct ConnectionError {
	ConnectionError(const std::string &msg, int errno_ = 0) :
		msg(msg), saved_errno(errno_)
	{
	}

	std::string msg;
	//Saved in case connection fails due to system error.
	int saved_errno = 0;
};

template <class BUFFER, class NetProvider>
class ProxyConnector;

template<class BUFFER, class NetProvider>
class ProxyConnection;

template<class BUFFER, class NetProvider>
struct ProxyConnectionImpl
{
private:
	//Only Connection can create instances of this class
	friend class ProxyConnection<BUFFER, NetProvider>;
	using iterator = typename BUFFER::iterator;

	ProxyConnectionImpl(ProxyConnector<BUFFER, NetProvider> &connector);
	ProxyConnectionImpl(const ProxyConnectionImpl& impl) = delete;
	ProxyConnectionImpl& operator = (const ProxyConnectionImpl& impl) = delete;
	~ProxyConnectionImpl() = default;

public:
    void ref();
	void unref();

	ProxyConnector<BUFFER, NetProvider> &connector;
	BUFFER clientToTntBuf;
	BUFFER tntToClientBuf;
	RequestDecoder<BUFFER> dec;
	/* Iterator separating decoded and raw data in input buffer. */
	iterator endDecoded;
	/* Network layer of the connection. */
	typename NetProvider::Stream_t client_strm;
    typename NetProvider::Stream_t tnt_strm;
    //Several connection wrappers may point to the same implementation.
	//It is useful to store connection objects in stl containers for example.
	ssize_t refs;
	//Members below can be default-initialized.
	std::optional<ConnectionError> error;
};

template<class BUFFER, class NetProvider>
ProxyConnectionImpl<BUFFER, NetProvider>::ProxyConnectionImpl(ProxyConnector<BUFFER, NetProvider> &conn) :
	connector(conn), clientToTntBuf(), tntToClientBuf(),
    dec(clientToTntBuf), endDecoded(clientToTntBuf.begin())
{
}

template<class BUFFER, class NetProvider>
void
ProxyConnectionImpl<BUFFER, NetProvider>::ref()
{
	assert(refs >= 0);
	refs++;
}

template<class BUFFER, class NetProvider>
void
ProxyConnectionImpl<BUFFER, NetProvider>::unref()
{
	assert(refs >= 1);
	if (--refs == 0)
		delete this;
}


/** Each connection is supposed to be bound to a single socket. */
template<class BUFFER, class NetProvider>
class ProxyConnection
{
public:
	using Impl_t = ProxyConnectionImpl<BUFFER, NetProvider>;

	ProxyConnection(ProxyConnector<BUFFER, NetProvider> &connector);
	ProxyConnection(Impl_t *a);
	~ProxyConnection();
	ProxyConnection(const ProxyConnection& connection);
	ProxyConnection& operator = (const ProxyConnection& connection);

	Impl_t *getImpl() { return impl; }
	const Impl_t *getImpl() const { return impl; }

	typename NetProvider::Stream_t &get_client_strm() { return impl->client_strm; }
    typename NetProvider::Stream_t &get_tnt_strm() { return impl->tnt_strm; }

    const typename NetProvider::Stream_t &get_client_strm() const { return impl->client_strm; }
	const typename NetProvider::Stream_t &get_tnt_strm() const { return impl->tnt_strm; }

	//Required for storing Connections in hash tables (std::unordered_map)
	friend bool operator == (const ProxyConnection<BUFFER, NetProvider>& lhs,
				 const ProxyConnection<BUFFER, NetProvider>& rhs)
	{
		return lhs.impl == rhs.impl;
	}

	//Required for storing Connections in trees (std::map)
	friend bool operator < (const ProxyConnection<BUFFER, NetProvider>& lhs,
				const ProxyConnection<BUFFER, NetProvider>& rhs)
	{
		// TODO: remove dependency on socket.
		return lhs.get_client_strm().get_fd() < rhs.get_client_strm().get_fd();
	}

	void setError(const std::string &msg, int errno_ = 0);
	bool hasError() const;
	ConnectionError& getError();
	void reset();

	BUFFER& getClientToTntBuf();
	BUFFER& getTntToClientBuf();

	template<class B, class N>
	friend
	void clientHasSentBytes(ProxyConnection<B, N> &conn, size_t bytes);

    template<class B, class N>
	friend
	void tntHasSentBytes(ProxyConnection<B, N> &conn, size_t bytes);

	template<class B, class N>
	friend
	void clientHasNotRecvBytes(ProxyConnection<B, N> &conn, size_t bytes);

    template<class B, class N>
	friend
	void tntHasNotRecvBytes(ProxyConnection<B, N> &conn, size_t bytes);

	template<class B, class N>
	friend
	bool clientHasDataToSend(ProxyConnection<B, N> &conn);

    template<class B, class N>
	friend
    bool tntHasDataToSend(ProxyConnection<B, N> &conn);

	template<class B, class N>
	friend
	bool clientHasDataToDecode(ProxyConnection<B, N> &conn);

	template<class B, class N>
	friend
	enum DecodeStatus processRequest(ProxyConnection<B, N> &conn,
					  Request<B> *result);

    template<class B, class N>
	friend
	int connectionDecodeRequests(ProxyConnection<B, N> &conn);

private:
	ProxyConnectionImpl<BUFFER, NetProvider> *impl;
};

template<class BUFFER, class NetProvider>
ProxyConnection<BUFFER, NetProvider>::ProxyConnection(ProxyConnector<BUFFER, NetProvider> &connector) :
				   impl(new ProxyConnectionImpl(connector))
{
    impl->ref();
}

template<class BUFFER, class NetProvider>
ProxyConnection<BUFFER, NetProvider>::ProxyConnection(ProxyConnectionImpl<BUFFER, NetProvider> *a) :
	impl(a)
{
	impl->ref();
}

template<class BUFFER, class NetProvider>
ProxyConnection<BUFFER, NetProvider>::ProxyConnection(const ProxyConnection& connection) :
	impl(connection.impl)
{
	impl->ref();
}

template<class BUFFER, class NetProvider>
ProxyConnection<BUFFER, NetProvider>&
ProxyConnection<BUFFER, NetProvider>::operator = (const ProxyConnection& other)
{
	if (this == &other)
		return *this;
	impl->unref();
	impl = other.impl;
	impl->ref();
	return *this;
}

template<class BUFFER, class NetProvider>
ProxyConnection<BUFFER, NetProvider>::~ProxyConnection()
{
	impl->unref();
}


template<class BUFFER, class NetProvider>
void
ProxyConnection<BUFFER, NetProvider>::setError(const std::string &msg, int errno_)
{
	impl->error.emplace(msg, errno_);
}

template<class BUFFER, class NetProvider>
bool
ProxyConnection<BUFFER, NetProvider>::hasError() const
{
	return impl->error.has_value();
}

template<class BUFFER, class NetProvider>
ConnectionError&
ProxyConnection<BUFFER, NetProvider>::getError()
{
	assert(hasError());
	return impl->error.value();
}

template<class BUFFER, class NetProvider>
void
ProxyConnection<BUFFER, NetProvider>::reset()
{
	impl->error.reset();
}

template<class BUFFER, class NetProvider>
BUFFER&
ProxyConnection<BUFFER, NetProvider>::getClientToTntBuf()
{
	return impl->clientToTntBuf;
}

template<class BUFFER, class NetProvider>
BUFFER&
ProxyConnection<BUFFER, NetProvider>::getTntToClientBuf()
{
	return impl->tntToClientBuf;
}

template<class BUFFER, class NetProvider>
void
clientHasSentBytes(ProxyConnection<BUFFER, NetProvider> &conn, size_t bytes)
{
	//dropBack()/dropFront() interfaces require number of bytes be greater
	//than zero so let's check it first.
	if (bytes > 0) {
		conn.impl->clientToTntBuf.dropFront(bytes);
    }
}

template<class BUFFER, class NetProvider>
void
tntHasSentBytes(ProxyConnection<BUFFER, NetProvider> &conn, size_t bytes)
{
	//dropBack()/dropFront() interfaces require number of bytes be greater
	//than zero so let's check it first.
	if (bytes > 0) {
		conn.impl->tntToClientBuf.dropFront(bytes);
    }
}

template<class BUFFER, class NetProvider>
void
clientHasNotRecvBytes(ProxyConnection<BUFFER, NetProvider> &conn, size_t bytes)
{
	if (bytes > 0) {
        conn.impl->clientToTntBuf.dropBack(bytes);
    }
}

template<class BUFFER, class NetProvider>
void
tntHasNotRecvBytes(ProxyConnection<BUFFER, NetProvider> &conn, size_t bytes)
{
	if (bytes > 0) {
		conn.impl->tntToClientBuf.dropBack(bytes);
    }
}

template<class BUFFER, class NetProvider>
bool
clientHasDataToSend(ProxyConnection<BUFFER, NetProvider> &conn)
{
	return !conn.impl->clientToTntBuf.empty();
}

template<class BUFFER, class NetProvider>
bool
tntHasDataToSend(ProxyConnection<BUFFER, NetProvider> &conn)
{
	return !conn.impl->tntToClientBuf.empty();
}

template<class BUFFER, class NetProvider>
bool
clientHasDataToDecode(ProxyConnection<BUFFER, NetProvider> &conn)
{
	assert(conn.impl->endDecoded < conn.impl->clientToTntBuf.end() ||
	       conn.impl->endDecoded == conn.impl->clientToTntBuf.end());
	return conn.impl->endDecoded != conn.impl->clientToTntBuf.end();
}

template<class BUFFER, class NetProvider>
DecodeStatus
processRequest(ProxyConnection<BUFFER, NetProvider> &conn,
		Request<BUFFER> *result)
{
	if (! conn.impl->clientToTntBuf.has(conn.impl->endDecoded, MP_REQUEST_SIZE)) {
		return DECODE_NEEDMORE;
    }

	Request<BUFFER> request;
	request.size = conn.impl->dec.decodeRequestSize();
	if (request.size < 0) {
		LOG_ERROR("Failed to decode request size");
        return DECODE_ERR;
	}
	request.size += MP_REQUEST_SIZE;
	if (! conn.impl->clientToTntBuf.has(conn.impl->endDecoded, request.size)) {
        conn.impl->dec.reset(conn.impl->endDecoded);
		return DECODE_NEEDMORE;
	}
	if (conn.impl->dec.decodeRequest(request) != 0) {
		LOG_ERROR("Failed to decode request");
		return DECODE_ERR;
	}

    *result = std::move(request);

    conn.impl->endDecoded += request.size;
    conn.impl->dec.reset(conn.impl->endDecoded);

	return DECODE_SUCC;
}

template<class BUFFER, class NetProvider>
int
connectionDecodeRequests(ProxyConnection<BUFFER, NetProvider> &conn)
{
	while (clientHasDataToDecode(conn)) {
        Request<BUFFER> request;
		DecodeStatus rc = processRequest(conn, &request);
		if (rc == DECODE_ERR)
			return -1;

		if (rc == DECODE_NEEDMORE)
			return 0;
		assert(rc == DECODE_SUCC);

        // logic
		conn.impl->connector.processRequest(std::move(request));

	}
	return 0;
}

