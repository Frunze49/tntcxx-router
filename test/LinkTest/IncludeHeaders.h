#pragma once
/*
 * Copyright 2010-2024, Tarantool AUTHORS, please see AUTHORS file.
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

#include "src/Buffer/Buffer.hpp"
#include "src/Client/Connection.hpp"
#include "src/Client/Connector.hpp"
#include "src/Client/IprotoConstants.hpp"
#include "src/Client/LibevNetProvider.hpp"
#include "src/Client/RequestEncoder.hpp"
#include "src/Client/MessageDecoder.hpp"
#include "src/Client/MessageReader.hpp"
#include "src/Client/Scramble.hpp"
#include "src/Client/Stream.hpp"
#include "src/Client/UnixPlainStream.hpp"
#include "src/Client/UnixStream.hpp"
#include "src/mpp/mpp.hpp"

#ifdef __linux__
#include "src/Client/EpollNetProvider.hpp"
#endif

#ifdef TNTCXX_ENABLE_SSL
#include "src/Client/UnixSSLStream.hpp"
#endif
