#include "message.pb.h"
#include "stub.h"
#include <arpa/inet.h>
#include <future>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
using namespace std;

int main() {
    {
        CalcServiceStub calc_stub;
        AddRequest add_req;
        int a = 1, b = 2;
        add_req.set_a(a);
        add_req.set_b(b);
        auto add_resp = calc_stub.add(add_req);

        SubtractRequest sub_req;
        sub_req.set_a(b);
        sub_req.set_b(a);
        auto sub_resp = calc_stub.subtract(sub_req);
        cout << a << " + " << b << " = " << add_resp.get().c() << endl;
        cout << b << " - " << a << " = " << sub_resp.get().c() << endl;
    }

    {
        EchoServiceStub echo_stub;
        EchoRequest echo_req;
        string req_msg = "echo success";
        echo_req.set_msg(req_msg);
        auto echo_resp = echo_stub.echo(echo_req);
        cout << echo_resp.get().msg() << endl;
    }
}

// g++ -std=c++23 example/client.cpp build/message.pb.cc src/stub.cpp src/rpc_conn_pool.cpp -o build/client.out -Ibuild -Iinclude -lprotobuf -lzookeeper_mt