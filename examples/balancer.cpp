#include "../src/Client/ProxyConnector.hpp"
#include "../src/Buffer/Buffer.hpp"

using Buf_t = tnt::Buffer<16 * 1024>;

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
            if (message.header.code == Iproto::REPLACE) {
                for (int i = 0; i <= 1 /* instances amount */; ++i) {
                    auto &strm = connect(i);
                    sendDecodedToStream(strm, message.size);
                }
                skipLastDecodedMessage(message.size);
            } else {
                auto &strm = connect(indexes[0]); // connect to random
                sendDecodedToStream(strm, message.size);
                skipLastDecodedMessage(message.size);
            }
        } else {
            sendDecodedToClient(message.size);
        }
    }
}

int main() {

    std::vector<ConnectOptions> opts;
    opts.push_back({.address = "127.0.0.1", .service = "3301", .is_tnt = true});
    opts.push_back({.address = "127.0.0.1", .service = "3302", .is_tnt = true});

    const std::string addr = "127.0.0.1";
    uint16_t port = 3304;

    ProxyConnector<Buf_t> proxy(opts, addr, port);
    
    proxy.start();
}