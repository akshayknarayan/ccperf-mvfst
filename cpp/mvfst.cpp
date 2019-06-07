#include "mvfst.hpp"

QuicClient::QuicClient(const char *host, u16 port) : addr(host, port), networkThread("CCPerfClientThread") {
    this->evb = networkThread.getEventBase();
}

std::future<int> QuicClient::connect() {
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

std::future<int> QuicClient::send(const void *data, u32 len) {
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

std::pair<quic::StreamId, std::future<int>> QuicClient::createStream(u32 len) {
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

void QuicClient::sendOnStream(quic::StreamId streamId, const void *data, u32 len) {
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

QuicServer::QuicServer(u16 port) : quicServer(quic::QuicServer::createQuicServer()) {
    std::ostringstream port_str_conv;
    port_str_conv << "127.0.0.1:" << port; // ugh
    std::string port_str = port_str_conv.str();
    LOG(INFO) << "Binding to " << port_str;
    addr.setFromLocalIpPort(port_str);
    quicServer->setFizzContext(quic::test::createServerCtx());
    quicServer->setQuicServerTransportFactory(std::make_unique<QuicServer::TransportFactory>());
}

void QuicServer::start() {
    quicServer->start(addr, 0);
    LOG(INFO) << "CCPerf server started";
    evb.loopForever();
}
