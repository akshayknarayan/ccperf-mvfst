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
    QuicClient(const std::string& host, u16 port): addr(host.c_str(), port), networkThread("CCPerfClientThread") {
        this->evb = networkThread.getEventBase();
    }

    std::future<int> connect() {
        this->evb->runInEventBaseThreadAndWait([&] {
            auto sock = std::make_unique<folly::AsyncUDPSocket>(evb);
            quicClient =
                std::make_shared<quic::QuicClientTransport>(evb, std::move(sock));
            quicClient->setHostname("echo.com");
            quicClient->setCertificateVerifier(
                quic::test::createTestCertificateVerifier()
            );
            quicClient->addNewPeerAddress(addr);

            LOG(INFO) << "CCPerfMvfst connecting to " << addr.describe();
            quicClient->start(this);
        });

        return transportReady.get_future();
    }

    std::future<int> send(void *data, u32 len) {
        quic::StreamId streamId;
        this->evb->runInEventBaseThreadAndWait([&] {
            if (auto ok = quicClient->createBidirectionalStream()) {
                streamId = ok.value();
            } else {
                LOG(ERROR) << "Could not createBidirectionalStream: " << ok.error();
                return;
            }

            pendingStreams[streamId].append(data, len);
            pendingStreamPromises[streamId] = std::promise<int>();
            
            // returns "Result"
            if (auto ok = quicClient->notifyPendingWriteOnStream(streamId, this); !ok) {
                LOG(ERROR) << "Could not notifyPendingWriteOnStream: " << ok.error();
                pendingStreamPromises[streamId].set_value(-1);
                return; 
            }

            // returns "Result"
            if (auto ok = quicClient->registerDeliveryCallback(streamId, len - 1, this); !ok) {
                LOG(ERROR) << "Could not registerDeliveryCallback: " << ok.error();
                pendingStreamPromises[streamId].set_value(-1);
                return;
            }
        });

        return pendingStreamPromises[streamId].get_future();
    }

    // In case caller doesn't want to allocate a massive data buffer
    // see also sendOnStream
    std::pair<quic::StreamId, std::future<int>> createStream(u32 len) {
        quic::StreamId streamId;
        this->evb->runInEventBaseThreadAndWait([&] {
            if (auto ok = quicClient->createBidirectionalStream()) {
                streamId = ok.value();
            } else {
                LOG(ERROR) << "Could not createBidirectionalStream: " << ok.error();
                return;
            }

            pendingStreamPromises[streamId] = std::promise<int>();
            if (auto ok = quicClient->registerDeliveryCallback(streamId, len - 1, this); !ok) {
                LOG(ERROR) << "Could not registerDeliveryCallback: " << ok.error();
                pendingStreamPromises[streamId].set_value(-1);
                return;
            }
        });

        VLOG(1) << "Created stream " << streamId;

        return std::pair(streamId, pendingStreamPromises[streamId].get_future());
    }
    
    void sendOnStream(quic::StreamId streamId, void *data, u32 len) {
        this->evb->runInEventBaseThreadAndWait([&] {
            pendingStreams[streamId].append(data, len);
            auto ok = quicClient->notifyPendingWriteOnStream(streamId, this);
            if (ok.hasError() && ok.error() != quic::LocalErrorCode::CALLBACK_ALREADY_INSTALLED) {
                LOG(ERROR) << "Could not notifyPendingWriteOnStream: " << streamId << " " << ok.error();
                pendingStreamPromises[streamId].set_value(-1);
                return; 
            } else if (ok.hasError()) {
                VLOG(5) << "notifyPendingWriteOnStream callback already installed ";
            }
            auto fc = quicClient->getConnectionFlowControl();
            if (!fc) {
                LOG(ERROR) << "getConnectionFlowConrol err: " << fc.error();
                return;
            }

            VLOG(10) 
                << " sendWindowAvailable=" << quicClient->getStreamFlowControl(streamId).value().sendWindowAvailable
                << " sendWindowMaxOffset=" << quicClient->getStreamFlowControl(streamId).value().sendWindowMaxOffset
                << " receiveWindowAvailable=" << quicClient->getStreamFlowControl(streamId).value().receiveWindowAvailable
                << " receiveWindowMaxOffset=" << quicClient->getStreamFlowControl(streamId).value().receiveWindowMaxOffset
                << " streamWriteOffset=" << quicClient->getStreamWriteOffset(streamId).value();
        });
    }

    //
    // ConnectionCallback
    //
    
    void onTransportReady() noexcept override {
        VLOG(1) << "Transport ready";
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
        VLOG(1) << "CCPerf stream done: " << streamId << " rtt " << rtt.count();
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

class QuicServer  {
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

    QuicServer(u16 port) : quicServer(quic::QuicServer::createQuicServer()) {
        std::ostringstream port_str_conv;
        port_str_conv << "0.0.0.0:" << port; // ugh
        std::string port_str = port_str_conv.str();
        LOG(INFO) << "Binding to " << port_str;
        addr.setFromLocalIpPort(port_str);
        quicServer->setFizzContext(quic::test::createServerCtx());
        quicServer->setQuicServerTransportFactory(std::make_unique<QuicServer::TransportFactory>());
    }

    void start() {
        quicServer->start(addr, 0);
        LOG(INFO) << "CCPerf server started";
        evb.loopForever();
    }

  private:
    folly::SocketAddress addr;
    folly::EventBase evb;
    std::shared_ptr<quic::QuicServer> quicServer;
};
