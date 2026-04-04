#include "message.pb.h"
#include "stub.h"
#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
using namespace std;

int main() {
    int fd = socket(PF_INET, SOCK_STREAM, 0);

    string ip = "127.0.0.1";
    int port = 9090;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.data(), &addr.sin_addr);
    connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));

    AddRequest req;
    int a = 1, b = 2;
    req.set_a(a);
    req.set_b(b);

    AddServiceStub add_stub(fd);
    int T = 2;
    while (T--) {
        AddResponse resp = add_stub.add(req);
        cout << a << " + " << b << " = " << resp.c() << endl;
    }

    close(fd);
}

// g++ example/client.cpp build/message.pb.cc src/stub.cpp -o build/client.out -Ibuild -Iinclude -lprotobuf