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
#include <cstdint>
#include <map>
#include <optional>
#include <tuple>

#include "IprotoConstants.hpp"
#include "RequestReader.hpp"
#include "../Utils/Logger.hpp"
#include "../Utils/Base64.hpp"

/** Size in bytes of encoded into msgpack size of packet*/
static constexpr size_t MP_REQUEST_SIZE = 5;

enum DecodeStatus {
	DECODE_SUCC = 0,
	DECODE_ERR = -1,
	DECODE_NEEDMORE = 1
};

template<class BUFFER>
class RequestDecoder {
public:
	RequestDecoder(BUFFER &buf) : buf_ref(buf), it(buf_ref.begin()) {};
	RequestDecoder(iterator_t<BUFFER> &itr) : it(itr) {};
	~RequestDecoder() { };
	RequestDecoder() = delete;
	RequestDecoder(const RequestDecoder& decoder) = delete;
	RequestDecoder& operator = (const RequestDecoder& decoder) = delete;

	int decodeRequest(Request<BUFFER> &request);
	int decodeRequestSize();
	void reset(iterator_t<BUFFER> &itr);

private:
    BUFFER& buf_ref;
	iterator_t<BUFFER> it;
};

template<class BUFFER>
int
RequestDecoder<BUFFER>::decodeRequestSize()
{
	int size = -1;
	bool ok = mpp::decode(it, size);
	//TODO: raise more detailed error
	if (!ok)
		return -1;
	return size;
}

template<class BUFFER>
int
RequestDecoder<BUFFER>::decodeRequest(Request<BUFFER> &request)
{
	/* Decode header and body separately to get more detailed error. */
	if (!mpp::decode(it, request.header)) {
		LOG_ERROR("Failed to decode header");
		return -1;
	}
	if (!mpp::decode(it, request.body)) {
		LOG_ERROR("Failed to decode body");
		return -1;
	}
	return 0;
}

template<class BUFFER>
void
RequestDecoder<BUFFER>::reset(iterator_t<BUFFER> &itr)
{
	it = itr;
}
