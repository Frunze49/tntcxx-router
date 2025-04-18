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
#include <optional>
#include <tuple>
#include <vector>
#include <variant>

#include "IprotoConstants.hpp"
#include "../mpp/mpp.hpp"
#include "../Utils/Logger.hpp"

template<class BUFFER>
using iterator_t = typename BUFFER::iterator;

struct RequestHeader {
	int code;
	int sync;

	static constexpr auto mpp = std::make_tuple(
		std::make_pair(Iproto::REQUEST_TYPE, &RequestHeader::code),
		std::make_pair(Iproto::SYNC, &RequestHeader::sync)
	);
};


template<class BUFFER>
struct Tuple {
	using it_t = iterator_t<BUFFER>;
	std::pair<it_t, it_t> iters;
	
	/** Unpacks tuples to passed container. */
	template<class T>
	bool decode(T& tuples)
	{
		it_t itr = iters.first;
		bool ok = mpp::decode(itr, tuples);
		assert(itr == iters.second);
		return ok;
	}

	static constexpr auto mpp = &Tuple<BUFFER>::iters;
};

template<class BUFFER>
struct RequestBody {

    std::optional<uint32_t> space_id;

    std::optional<uint32_t> index_id;
    std::optional<uint32_t> limit;
    std::optional<uint32_t> offset;
    std::optional<uint32_t> iterator;

    std::optional<Tuple<BUFFER>> keys;
    std::optional<Tuple<BUFFER>> tuple;

	static constexpr auto mpp = std::make_tuple(
        std::make_pair(Iproto::SPACE_ID, &RequestBody::space_id),

        std::make_pair(Iproto::INDEX_ID, &RequestBody<BUFFER>::index_id),
        std::make_pair(Iproto::LIMIT, &RequestBody<BUFFER>::limit),
        std::make_pair(Iproto::OFFSET, &RequestBody<BUFFER>::offset),
        std::make_pair(Iproto::ITERATOR, &RequestBody<BUFFER>::iterator),

        std::make_pair(Iproto::KEY, &RequestBody<BUFFER>::keys),
        std::make_pair(Iproto::TUPLE, &RequestBody<BUFFER>::tuple)
	);
};

template<class BUFFER>
struct Request {
	RequestHeader header;
	RequestBody<BUFFER> body;
	int size;
};

template <class BUFFER, class CustomTuple>
CustomTuple decodeCustomTuple(Tuple<BUFFER> &data) {
	CustomTuple results;
	bool ok = data.decode(results);
	(void)ok;
	assert(ok);
	return results;
}