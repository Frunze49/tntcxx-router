#include "../src/Client/UnixServer.hpp"

int main() {

    UnixProxy proxy("127.0.0.1", 3302, "127.0.0.1", 3301);
    
    if (proxy.start() != 0) {
        std::cerr << "Failed to start proxy" << std::endl;
        return 1;
    }
    
    std::cout << "Proxy running. Press Enter to stop..." << std::endl;
    std::cin.get();
    
    proxy.stop();
    return 0;
}