#include "message.pb.h"
#include "stub.h"
#include <arpa/inet.h>
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
        AddResponse add_resp = calc_stub.add(add_req);
        cout << a << " + " << b << " = " << add_resp.c() << endl;

        SubtractRequest sub_req;
        sub_req.set_a(b);
        sub_req.set_b(a);
        SubtractResponse sub_resp = calc_stub.subtract(sub_req);
        cout << b << " - " << a << " = " << sub_resp.c() << endl;
    }

    {
        EchoServiceStub echo_stub;
        EchoRequest echo_req;
        string req_msg = "echo success";
        echo_req.set_msg(req_msg);
        EchoResponse echo_resp = echo_stub.echo(echo_req);
        cout << echo_resp.msg() << endl;
    }
}

// g++ example/client.cpp build/message.pb.cc src/stub.cpp -o build/client.out -Ibuild -Iinclude -lprotobuf -lzookeeper_mt