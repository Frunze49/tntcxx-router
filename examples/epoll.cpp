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

void CustomRequestProcess(Request<Buf_t> request) {
    LOG_DEBUG("Header: sync=", request.header.sync, ", code=", request.header.code);
    if (!request.body.space_id) {
        LOG_ERROR("Missing space_id in request");
        return;
    }
    unsigned int space_id = *request.body.space_id; // Разыменовываем optional
    LOG_DEBUG("Body: space_id = ", space_id);
	if (request.header.code == Iproto::SELECT) {
		Tuple<Buf_t>& keys = *request.body.keys;
		KeyTuple key_tuple = decodeCustomTuple<Buf_t, KeyTuple>(keys);
        LOG_DEBUG("Key: ", key_tuple.key);
	} else if (request.header.code == Iproto::REPLACE) {
        Tuple<Buf_t>& tuple = *request.body.tuple;
		RowTuple replace_tuple = decodeCustomTuple<Buf_t, RowTuple>(tuple);
        LOG_DEBUG("Replace tuple: field1 - ", replace_tuple.field1, ", field2 - ",
                  replace_tuple.field2, ", field3 - ", replace_tuple.field3);
    }
}

// Client1          //
             // <- Proxy ->    // TNt
// Client2


int main() {

    ConnectOptions opts{.address = "127.0.0.1", .service = "3301"};
    const std::string addr = "127.0.0.1";
    uint16_t port = 3302;

    ProxyConnector<Buf_t> proxy(opts, addr, port, &CustomRequestProcess);
    
    if (proxy.start() != 0) {
        std::cerr << "Failed to start proxy" << std::endl;
        return 1;
    }
    
    std::cout << "Proxy running. Press Enter to stop..." << std::endl;
    std::cin.get();
    
    proxy.stop();
    return 0;
}