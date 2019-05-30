#include "mvfst.hpp"

#include <gflags/gflags.h>
#include <fizz/crypto/Utils.h>

DEFINE_string(mode, "client", "Send (client) or receive (server) traffic");
DEFINE_string(ip, "127.0.0.1", "IP to connect to");
DEFINE_int32(port, 4242, "Port to listen on/send to");
DEFINE_int32(bytes, 100000, "Number of bytes to transfer");

void do_client() {
    auto client = QuicClient(FLAGS_ip, FLAGS_port);
    LOG(INFO) << "connecting";
    auto ready = client.connect();
    ready.wait();

    LOG(INFO) << "sending";
    auto create_stream = client.createStream(FLAGS_bytes);
    auto streamId = create_stream.first;
    auto on_done = std::move(create_stream.second);

    const u32 CHUNK_SIZE = 1024;
    void *data = calloc(CHUNK_SIZE, sizeof(char));
    if (data == NULL) {
        LOG(ERROR) << "Could not allocate send buffer";
        return;
    }

    auto num_writes = ((FLAGS_bytes % CHUNK_SIZE) == 0) ? (FLAGS_bytes / CHUNK_SIZE) : ((FLAGS_bytes / CHUNK_SIZE) + 1);
    while (num_writes -- > 0) {
        LOG(INFO) << "writing " << num_writes << " chunks";
        client.sendOnStream(streamId, data, CHUNK_SIZE);
    }

    LOG(INFO) << "waiting";
    int ok = on_done.get();
    if (ok < 0) {
        LOG(ERROR) << "send failed";
    }

    LOG(INFO) << "done";
}

void do_server() {
    auto server = QuicServer(FLAGS_port);
    server.start();
}

int main(int argc, char **argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    fizz::CryptoUtils::init();

    if (FLAGS_mode == "client") {
        do_client();
    } else {
        do_server();
    }

    return 0;
}
