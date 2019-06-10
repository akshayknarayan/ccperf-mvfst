#include "mvfst.hpp"

#include <gflags/gflags.h>
#include <fizz/crypto/Utils.h>
#include <chrono>
#include <ctime>

DEFINE_string(mode, "client", "Send (client) or receive (server) traffic");
DEFINE_string(ip, "127.0.0.1", "IP to connect to");
DEFINE_int32(port, 4242, "Port to listen on/send to");
DEFINE_int32(bytes, 100000, "Number of bytes to transfer");
DEFINE_int32(duration, 0, "Duration in seconds to run transfer for");

void do_client_bytes(QuicClient *client, int bytes) {
    auto create_stream = client->createStream(bytes);
    auto streamId = create_stream.first;
    auto on_done = std::move(create_stream.second);

    const u32 CHUNK_SIZE = 1024;
    void *data = calloc(CHUNK_SIZE, sizeof(char));
    if (data == NULL) {
        LOG(ERROR) << "Could not allocate send buffer";
        return;
    }

    auto num_writes = ((bytes % CHUNK_SIZE) == 0) ? (bytes / CHUNK_SIZE) : ((bytes / CHUNK_SIZE) + 1);
    while (num_writes -- > 0) {
        client->sendOnStream(streamId, data, CHUNK_SIZE);
    }

    int ok = on_done.get();
    if (ok < 0) {
        LOG(ERROR) << "send failed";
    }
}

void do_client_duration(QuicClient *client, int seconds) {
    auto start = std::chrono::system_clock::now();
    auto now = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = now - start;
    size_t count = 0;
    while (elapsed_seconds.count() < seconds) {
        do_client_bytes(client, FLAGS_bytes);
        count++;

        now = std::chrono::system_clock::now();
        elapsed_seconds = now - start;
    }

    LOG(INFO) << "Transferred " 
        << count * FLAGS_bytes << " bytes"
        << " in " 
        << elapsed_seconds.count() << "s: " 
        << (count * FLAGS_bytes) / elapsed_seconds.count() << " Bps";
}

void do_client() {
    auto client = QuicClient(FLAGS_ip, FLAGS_port);
    auto ready = client.connect();
    ready.wait();

    if (FLAGS_duration > 0) {
        do_client_duration(&client, FLAGS_duration);
    } else {
        do_client_bytes(&client, FLAGS_bytes);
    }
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
