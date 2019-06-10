#pragma once

#include <future>

#include <glog/logging.h>

#include <folly/io/async/ScopedEventBaseThread.h>

#include <quic/api/QuicSocket.h>
#include <quic/client/QuicClientTransport.h>
#include <quic/common/test/TestUtils.h>
#include <quic/server/QuicServer.h>
#include <quic/server/QuicServerTransport.h>
#include <quic/server/QuicSharedUDPSocketFactory.h>

typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

class QuicClient : public quic::QuicSocket::ConnectionCallback,
                   public quic::QuicSocket::ReadCallback,
                   public quic::QuicSocket::WriteCallback,
                   public quic::QuicSocket::DeliveryCallback
{
  public:
    // 
    // external API
    //
    
    QuicClient(const char *host, u16 port);

    std::future<int> connect();
    std::future<int> send(const void *data, u32 len);

    // In case caller doesn't want to allocate a massive data buffer
    // see also sendOnStream
    std::pair<quic::StreamId, std::future<int>> createStream(u32 len);
    void sendOnStream(quic::StreamId streamId, const void *data, u32 len);

    //
    // ConnectionCallback
    //
    
    void onTransportReady() noexcept override {
        LOG(INFO) << "Transport ready";
        transportReady.set_value(0);
    }
    
    void onNewBidirectionalStream(quic::StreamId id) noexcept override {
        LOG(INFO) << "Created stream " << id;
        quicClient->setReadCallback(id, this);
    }

    //meh 
    void onNewUnidirectionalStream(quic::StreamId id) noexcept override { assert(false); }

    void onStopSending(
        quic::StreamId id,
        quic::ApplicationErrorCode error
    ) noexcept override {
        LOG(INFO) << "Got StopSending message";
    }

    void onConnectionEnd() noexcept override {
        LOG(INFO) << "Connection ended.";
    }

    void onConnectionError(
        std::pair<quic::QuicErrorCode, std::string> error
    ) noexcept override {
        LOG(ERROR) << "CCPerf client connection error=" << error.second;
    }

    // 
    // ReadCallback
    //

    void readAvailable(quic::StreamId id) noexcept override {
        LOG(INFO) << "Read on stream " << id;
    }

    void readError(
        quic::StreamId id, 
        std::pair<quic::QuicErrorCode, folly::Optional<folly::StringPiece>> err
    ) noexcept override {
        LOG(ERROR) << "CCPerf client read error=" << err;
    }

    // 
    // WriteCallback
    //
    
    void onStreamWriteReady(
        quic::StreamId streamId,
        u64 maxToSend
    ) noexcept override {
        auto folly_buf = &pendingStreams[streamId];
        auto ok = quicClient->writeChain(streamId, std::move(folly_buf->move()), false, true);
        if (ok.hasError()) {
            LOG(ERROR) << "CCPerf client writeChain stream=" << streamId << " error=" << uint32_t(ok.error());
        } else if (ok.value()) { // quic didn't accept all the data we gave it
            LOG(INFO) << "send buffer space exceeded" << streamId;
            folly_buf->append(std::move(ok.value()));
            quicClient->notifyPendingWriteOnStream(streamId, this);
        } else { // send ok
            pendingStreams.erase(streamId);
        }
    }

    void onStreamWriteError(
        quic::StreamId id,
        std::pair<quic::QuicErrorCode, folly::Optional<folly::StringPiece>> err
    ) noexcept override {
        LOG(ERROR) << "CCPerf client write error=" << err;
    }

    void onConnectionWriteReady( uint64_t maxToSend) noexcept override { /* unused */ }

    void onConnectionWriteError(
        std::pair<quic::QuicErrorCode, folly::Optional<folly::StringPiece>> error
    ) noexcept override { /* unused */ }

    // 
    // DeliverCallback
    //

    void onDeliveryAck(
        quic::StreamId streamId,
        u64 offset,
        std::chrono::microseconds rtt
    ) noexcept override {
        LOG(INFO) << "CCPerf stream done: " << streamId << " rtt " << rtt.count();
        pendingStreamPromises[streamId].set_value(0);
        pendingStreamPromises.erase(streamId);
        quicClient->shutdownWrite(streamId);
    }

    void onCanceled(quic::StreamId id, u64 offset) noexcept override {
        LOG(ERROR) << "CCPerf client stream failed:" << id;
        pendingStreamPromises[id].set_value(-1);
        pendingStreamPromises.erase(id);
    }

	~QuicClient() override = default;

  private:
    folly::SocketAddress addr;
    std::shared_ptr<quic::QuicClientTransport> quicClient;
    folly::ScopedEventBaseThread networkThread;
    folly::EventBase *evb;
    std::map<quic::StreamId, folly::IOBufQueue> pendingStreams;
    std::map<quic::StreamId, std::promise<int>> pendingStreamPromises;
    std::promise<int> transportReady;
};

class QuicServer {
  public:
    class Connection : public quic::QuicSocket::ConnectionCallback,
                       public quic::QuicSocket::ReadCallback 
    {
      public:
        Connection(folly::EventBase *e) : evb(e) {}

        void setQuicSocket(std::shared_ptr<quic::QuicSocket> sock) {
            quicSocket = sock;
        }

        folly::EventBase* getEventBase() {
            return evb;
        }

        //
        // ConnectionCallback
        //

        void onNewBidirectionalStream(quic::StreamId id) noexcept override {
            VLOG(1) << "New stream " << id;
            quicSocket->setReadCallback(id, this);
        }

        //meh 
        void onNewUnidirectionalStream(quic::StreamId id) noexcept override { assert(false); }
    
        void onStopSending(
            quic::StreamId id,
            quic::ApplicationErrorCode error
        ) noexcept override {
            LOG(INFO) << "Done sending";
        }

        void onConnectionEnd() noexcept override {
            LOG(INFO) << "Connection ended.";
        }

        void onConnectionError(
            std::pair<quic::QuicErrorCode, std::string> error
        ) noexcept override {
            VLOG(5) << "CCPerf server connection error=" << error.second;
        }

        // 
        // ReadCallback
        //

        void readAvailable(quic::StreamId id) noexcept override {
            if (auto ok = quicSocket->read(id, 0); !ok) {
                LOG(ERROR) << id << "read error " << ok.error();
            } // throw the read bytes away
        }

        void readError(
            quic::StreamId id, 
            std::pair<quic::QuicErrorCode, folly::Optional<folly::StringPiece>> err
        ) noexcept override {
            VLOG(5) << "CCPerf server read error=" << err << " streamId=" << id;
        }

      private:
        folly::EventBase *evb;
        std::shared_ptr<quic::QuicSocket> quicSocket;
    };

    class TransportFactory: public quic::QuicServerTransportFactory {
      public:
        quic::QuicServerTransport::Ptr make(
            folly::EventBase *evb,
            std::unique_ptr<folly::AsyncUDPSocket> sock,
            const folly::SocketAddress&,
            std::shared_ptr<const fizz::server::FizzServerContext> ctx
        ) noexcept override {
            assert(evb == sock->getEventBase());
            auto connectionHandler = std::make_unique<QuicServer::Connection>(evb);
            auto transport = quic::QuicServerTransport::make(evb, std::move(sock), *connectionHandler, ctx);
            connectionHandler->setQuicSocket(transport);
            connectionHandlers.push_back(std::move(connectionHandler));
            return transport;
        }

        ~TransportFactory() override {
            while (!connectionHandlers.empty()) {
                auto& h = connectionHandlers.back();
                h->getEventBase()->runImmediatelyOrRunInEventBaseThreadAndWait([this] {
                    connectionHandlers.pop_back();
                });
            }
        }

        std::vector<std::unique_ptr<QuicServer::Connection>> connectionHandlers;
    };

    // 
    // external API
    //

    QuicServer(u16 port);
    void start();

  private:
    folly::SocketAddress addr;
    folly::EventBase evb;
    std::shared_ptr<quic::QuicServer> quicServer;
};
