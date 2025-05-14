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

#include "MessageDecoder.hpp"
#include "RequestEncoder.hpp"

#include "Stream.hpp"
#include "../Utils/Logger.hpp"

#include <sys/uio.h> //iovec
#include <string>
#include <unordered_map> //futures

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
	BUFFER decBuffer;
	BUFFER encBuffer;
	MessageDecoder<BUFFER> dec;
	RequestEncoder<BUFFER> enc;
	/* Iterator separating decoded and raw data in input buffer. */
	iterator endDecoded;
	/* Network layer of the connection. */
	typename NetProvider::Stream_t client_strm;
	std::map<int, typename NetProvider::Stream_t> instance_index_to_strm;
    //Several connection wrappers may point to the same implementation.
	//It is useful to store connection objects in stl containers for example.
	ssize_t refs;
	//Members below can be default-initialized.
	std::optional<ConnectionError> error;
};

template<class BUFFER, class NetProvider>
ProxyConnectionImpl<BUFFER, NetProvider>::ProxyConnectionImpl(ProxyConnector<BUFFER, NetProvider> &conn) :
	connector(conn), decBuffer(), encBuffer(),
    dec(decBuffer), enc(encBuffer), endDecoded(decBuffer.begin())
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
	std::map<int, typename NetProvider::Stream_t> &get_external_strms() { return impl->instance_index_to_strm; }
	bool is_connected_to_instance(int index) {
		auto it = impl->instance_index_to_strm.find(index);
		if (it == impl->instance_index_to_strm.end()) {
			return false;
		}
		return true;
	}
	std::optional<typename NetProvider::Stream_t&> get_instance_strm(int index) {
		auto it = impl->instance_index_to_strm.find(index);
		if (it == impl->instance_index_to_strm.end()) {
			return std::nullopt;
		}
		return impl->instance_index_to_strm[index];
	}
	std::vector<int> get_connected_instances() {
		std::vector<int> indexes;
		for (auto it = impl->instance_index_to_strm.begin(); it != impl->instance_index_to_strm.end(); ++it) {
			indexes.push_back(it->first);
		}
		return indexes;
	}

    const typename NetProvider::Stream_t &get_client_strm() const { return impl->client_strm; }

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

	BUFFER& getDecBuf();
	BUFFER& getEncBuf();

	template<class B, class N>
	friend
	void hasSentDecodedData(ProxyConnection<B, N> &conn, size_t bytes);

	template<class B, class N>
	friend
	void hasSentEncodedData(ProxyConnection<B, N> &conn, size_t bytes);

	template<class B, class N>
	friend
	void hasNotRecvBytes(ProxyConnection<B, N> &conn, size_t bytes);

	template<class B, class N>
	friend
	bool hasDecodedDataToSend(ProxyConnection<B, N> &conn);

	template<class B, class N>
	friend
	bool hasEncodedDataToSend(ProxyConnection<B, N> &conn);

	template<class B, class N>
	friend
	bool hasDataToDecode(ProxyConnection<B, N> &conn);

	template<class B, class N>
	friend
	enum DecodeStatus processMessage(ProxyConnection<B, N> &conn,
					  Message<B> *result);

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
ProxyConnection<BUFFER, NetProvider>::getDecBuf()
{
	return impl->decBuffer;
}

template<class BUFFER, class NetProvider>
BUFFER&
ProxyConnection<BUFFER, NetProvider>::getEncBuf()
{
	return impl->encBuffer;
}

template<class BUFFER, class NetProvider>
void
hasSentDecodedData(ProxyConnection<BUFFER, NetProvider> &conn, size_t bytes)
{
	//dropBack()/dropFront() interfaces require number of bytes be greater
	//than zero so let's check it first.
	if (bytes > 0) {
		conn.impl->decBuffer.dropFront(bytes);
    }
}

template<class BUFFER, class NetProvider>
void
hasSentEncodedData(ProxyConnection<BUFFER, NetProvider> &conn, size_t bytes)
{
	//dropBack()/dropFront() interfaces require number of bytes be greater
	//than zero so let's check it first.
	if (bytes > 0) {
		conn.impl->encBuffer.dropFront(bytes);
    }
}

template<class BUFFER, class NetProvider>
void
hasNotRecvBytes(ProxyConnection<BUFFER, NetProvider> &conn, size_t bytes)
{
	conn.impl->decBuffer.dropBack(bytes);
}

template<class BUFFER, class NetProvider>
bool
hasDecodedDataToSend(ProxyConnection<BUFFER, NetProvider> &conn)
{
	return !conn.impl->decBuffer.empty();
}

template<class BUFFER, class NetProvider>
bool
hasEncodedDataToSend(ProxyConnection<BUFFER, NetProvider> &conn)
{
	return !conn.impl->encBuffer.empty();
}

template<class BUFFER, class NetProvider>
bool
hasDataToDecode(ProxyConnection<BUFFER, NetProvider> &conn)
{
	assert(conn.impl->endDecoded < conn.impl->decBuffer.end() ||
	       conn.impl->endDecoded == conn.impl->decBuffer.end());
	return conn.impl->endDecoded != conn.impl->decBuffer.end();
}

template<class BUFFER, class NetProvider>
DecodeStatus
processMessage(ProxyConnection<BUFFER, NetProvider> &conn,
		Message<BUFFER> *result)
{
	if (! conn.impl->decBuffer.has(conn.impl->endDecoded, MP_RESPONSE_SIZE)) {
		return DECODE_NEEDMORE;
    }

	Message<BUFFER> message;
	message.size = conn.impl->dec.decodeMessageSize();
	if (message.size < 0) {
		LOG_ERROR("Failed to decode message size");
        return DECODE_ERR;
	}
	message.size += MP_RESPONSE_SIZE;
	if (! conn.impl->decBuffer.has(conn.impl->endDecoded, message.size)) {
        conn.impl->dec.reset(conn.impl->endDecoded);
		return DECODE_NEEDMORE;
	}
	if (conn.impl->dec.decodeMessage(message) != 0) {
		LOG_ERROR("Failed to decode message");
		conn.impl->endDecoded += message.size;
		return DECODE_ERR;
	}

    *result = std::move(message);

    conn.impl->endDecoded += message.size;
    conn.impl->dec.reset(conn.impl->endDecoded);

	return DECODE_SUCC;
}
