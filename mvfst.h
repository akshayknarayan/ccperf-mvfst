#include <future>

namespace quic {
    using StreamId = uint64_t;
}

class QuicClient {
  public:
    // 
    // external API
    //
    
    QuicClient(const std::string& host, u16 port);

    std::future<int> connect();
    std::future<int> send(void *data, u32 len);

    // In case caller doesn't want to allocate a massive data buffer
    // see also sendOnStream
    std::pair<quic::StreamId, std::future<int>> createStream(u32 len);
    void sendOnStream(quic::StreamId streamId, void *data, u32 len);
};

class QuicServer {
  public:
    // 
    // external API
    //

    QuicServer(u16 port);
    void start();
};
