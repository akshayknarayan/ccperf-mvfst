#include "mvfst.hpp"

#include <gflags/gflags.h>

DEFINE_string(mode, "client", "Send (client) or receive (server) traffic");
DEFINE_string(ip, "127.0.0.1", "IP to connect to");
DEFINE_int32(port, 4242, "Port to listen on/send to");
DEFINE_int32(bytes, 1000000, "Number of bytes to transfer");

void do_client() {
    auto client = QuicClient(FLAGS_ip, FLAGS_port);
    client.connect();

    void *data = calloc(1024, sizeof(char));
    LOG(INFO) << "sending";
    auto on_done = client.send(data, 1024);
    LOG(INFO) << "waiting";

    on_done.wait();
    LOG(INFO) << "done";
}

void do_server() {
    auto server = QuicServer(FLAGS_port);
    server.start();
}

int main(int argc, char **argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    if (FLAGS_mode == "client") {
        do_client();
    } else {
        do_server();
    }

    return 0;
}
