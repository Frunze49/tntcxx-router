#include "../src/Client/ProxyConnector.hpp"
#include "../src/Buffer/Buffer.hpp"

using Buf_t = tnt::Buffer<16 * 1024>;

struct KeyTuple  {
    uint64_t key;

	static constexpr auto mpp = std::make_tuple(&KeyTuple::key);
};

struct RowTuple  {
    uint64_t field1;
    std::string field2;
	double field3;

    static constexpr auto mpp = std::make_tuple(    
		&RowTuple::field1, &RowTuple::field2, &RowTuple::field3);
};

template<class BUFFER, class NetProvider>
void
ProxyConnector<BUFFER, NetProvider>::customHandler() {
    if (isGreetingExpected()) {
        deliverDecodedGreeting();
    }
    while (auto iter = getNextDecodedMessage())
    {
        if (!iter) {
            break;
        }
        auto &message = iter.value();


        if (isRecvFromClient()) {
            if (message.header.code == Iproto::SELECT) {
                auto &keys = *message.body.keys;
                KeyTuple key_tuple = decodeCustomTuple<Buf_t, KeyTuple>(keys);
                LOG_ERROR("KEY: ", key_tuple.key);
            }
            auto &strm = connect(0);
            sendDecodedToStream(strm, message.size);
            skipLastDecodedMessage(message.size);
            
        } else {
            LOG_ERROR("Message size : ", message.size);
            LOG_ERROR("Header: req: ", message.header.code, ", sync: ", message.header.sync, ", schema: ", message.header.schema_id.value());
            sendDecodedToClient(message.size);
            skipLastDecodedMessage(message.size);
        }
    }
}

int main() {

    std::vector<ConnectOptions> opts;
    opts.push_back({.address = "127.0.0.1", .service = "3301", .is_tnt = true});

    const std::string addr = "127.0.0.1";
    uint16_t port = 3304;

    ProxyConnector<Buf_t> proxy(opts, addr, port);
    
    proxy.start();
}