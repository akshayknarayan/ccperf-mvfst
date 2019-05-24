#pragma once

#include <glog/logging.h>

#include <folly/io/async/ScopedEventBaseThread.h>

#include <quic/api/QuicSocket.h>
#include <quic/client/QuicClientTransport.h>
#include <quic/common/test/TestUtils.h>

typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

class QuicConnection : public quic::QuicSocket::ConnectionCallback {
  public:
    QuicConnection(const std::string& host, u16 port): addr(host.c_str(), port) {}

    void connect() {
        folly::ScopedEventBaseThread networkThread("CCPerfClientThread");
        this->evb = networkThread.getEventBase();
        
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
    }

    void send(void *data, u32 len) {
        this->evb->runInEventBaseThread([=] {
            // make new quic stream
            auto streamId = quicClient->createBidirectionalStream().value();
            quicClient->setReadCallback(streamId, NULL /* TODO pass read callback */);
            auto folly_buf = std::make_unique<folly::IOBufQueue>();
            folly_buf->append(data, len);

            auto ok = quicClient->writeChain(streamId, std::move(folly_buf->move()), true, false);
            if (ok.hasError()) {
                LOG(ERROR) << "EchoClient writeChain error=" << uint32_t(ok.error());
            } else if (ok.value()) { // quic didn't accept all the data we gave it
                folly_buf->append(std::move(ok.value()));
                quicClient->notifyPendingWriteOnStream(streamId, NULL /* TODO pass write callback */);
            } else { // send ok
                LOG(INFO) << "Wrote " << len << "bytes";
            }
        });
    }

    void onNewBidirectionalStream(quic::StreamId id) noexcept override {
        quicClient->setReadCallback(id, NULL /* TODO pass read callback */);
    }

    //meh 
    void onNewUnidirectionalStream(quic::StreamId id) noexcept override {}

    void onStopSending(
        quic::StreamId id,
        quic::ApplicationErrorCode error
    ) noexcept override {
    }

    void onConnectionEnd() noexcept override {
    }

    void onConnectionError(
        std::pair<quic::QuicErrorCode, std::string> error
    ) noexcept override {
    }

	~QuicConnection() override = default;

  private:
    folly::SocketAddress addr;
    std::shared_ptr<quic::QuicClientTransport> quicClient;
    folly::EventBase *evb;
};
