#include <future>

namespace quic {
    using StreamId = uint64_t;
}

class QuicClient {
  public:
    // 
    // external API
    //
    
    QuicClient(const std::string& host, uint16_t port);

    std::future<int> connect();
    std::future<int> send(void *data, uint32_t len);

    // In case caller doesn't want to allocate a massive data buffer
    // see also sendOnStream
    std::pair<quic::StreamId, std::future<int>> createStream(uint32_t len);
    void sendOnStream(quic::StreamId streamId, void *data, uint32_t len);
};

class QuicServer {
  public:
    // 
    // external API
    //

    QuicServer(uint16_t port);
    void start();
};
