#include "../src/Client/ProxyConnector.hpp"
#include "../src/Buffer/Buffer.hpp"


void create_test_greeting(char* buf) {
    std::string version_line = "Tarantool 2.10.0";
    version_line.resize(Iproto::GREETING_LINE1_SIZE - 1, ' ');
    version_line += '\n';

    std::string salt = "QK2HoFZGXTXBq2vFj7soCsHqTo6PGTF575ssUBAJLAI=";
    salt.resize(Iproto::GREETING_LINE2_SIZE - 1, ' ');
    salt += '\n';

    memcpy(buf, version_line.data(), Iproto::GREETING_LINE1_SIZE);
    memcpy(buf + Iproto::GREETING_LINE1_SIZE, salt.data(), Iproto::GREETING_LINE2_SIZE);
}

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


std::set<int> custom_greeting_accepted_on_fd;

template<class BUFFER, class NetProvider>
void
ProxyConnector<BUFFER, NetProvider>::customHandler() {
    if (isClientFirstRequest()) {
        LOG_ERROR("Deliver greeting");
        char greeting_buf[Iproto::GREETING_SIZE];
        create_test_greeting(greeting_buf);     
        deliverEncodedGreeting(greeting_buf);
    }
    while (auto iter = getNextDecodedMessage())
    {
        if (!iter) {
            break;
        }
        auto &message = iter.value();
        LOG_ERROR("Decode message: req: ", message.header.code, ", size: ", message.size);

        if (isRecvFromClient()) {
            int code = message.header.code;
            int sync = message.header.sync;

            if (code == Iproto::PING) {
                int size = createMessage(sync, 85);
                sendEncodedToClient(size);
            } else if (code == Iproto::REPLACE) {
                std::vector<std::tuple<int, std::string, double>> data;
                data.push_back(std::make_tuple(666, "111", 1.01));

                int size = createMessage(sync, 85, &data);
                sendEncodedToClient(size);
            } else if (code == Iproto::SELECT) {
                std::vector<std::tuple<int, std::string, double>> data;
                data.push_back(std::make_tuple(666, "111", 1.01));

                int size = createMessage(sync, 85, &data);
                sendEncodedToClient(size);
            }
            
        } else {
            std::abort();
        }
    }
}

int main() {
    const std::string addr = "127.0.0.1";
    uint16_t port = 3304;

    ProxyConnector<Buf_t> proxy({}, addr, port);
    
    proxy.start();
}