#include "impl.h"
#include "message.pb.h"
#include "rpc_server.h"
#include "status_code.h"
#include <string>
using namespace std;

int main() {
    RpcServer rpc_server;
    CalcServiceImpl calc_impl(&rpc_server);
    EchoServiceImpl echo_impl(&rpc_server);
    rpc_server.start();
}

// g++ -std=c++20 example/server.cpp build/message.pb.cc src/rpc_server.cpp src/impl.cpp -o build/server.out -Ibuild -Iinclude - lprotobuf