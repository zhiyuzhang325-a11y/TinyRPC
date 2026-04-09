#include "logger.h"
#include "message.pb.h"
#include "rpc_server.h"
#include "service_impl.h"
#include "status_code.h"
#include <signal.h>
#include <string>
using namespace std;

RpcServer *g_server = nullptr;

void handleSignal(int sig) {
    if (g_server) {
        LOG_INFO("server stop\n");
        g_server->stop();
    }
}

int main() {
    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);
    AsyncLogger::setLevel(AsyncLogger::DEBUG);

    RpcServer rpc_server;
    g_server = &rpc_server;
    CalcServiceImpl calc_impl(&rpc_server);
    EchoServiceImpl echo_impl(&rpc_server);
    rpc_server.registerToZk();
    rpc_server.start();
}

// g++ -std=c++20 example/server.cpp build/message.pb.cc src/rpc_server.cpp src/impl.cpp src/logger.cpp -o build/server.out -Ibuild -Iinclude -lprotobuf -lzookeeper_mt