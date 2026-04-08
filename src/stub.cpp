#include "stub.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
using namespace std;

RpcStub::RpcStub(const string &service_name) {
    m_service_name = service_name;

    m_zh = zookeeper_init("127.0.0.1:2181", nullptr, 5000, nullptr, nullptr, 0);

    string path = "/TinyRPC/CalcService";
    char buf[64];
    int buf_len = 64;
    int ret = zoo_get(m_zh, path.data(), 0, buf, &buf_len, nullptr);
    string ip;
    int port;
    if (ret == ZOK) {
        string addr(buf, buf_len);
        size_t pos = addr.find(':');
        ip = addr.substr(0, pos);
        port = stoi(addr.substr(pos + 1, buf_len - pos));
    } else {
        cout << "get failed, code: " << ret << endl;
        return;
    }

    m_fd = socket(PF_INET, SOCK_STREAM, 0);

    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(m_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.data(), &addr.sin_addr);
    connect(m_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
}

RpcStub::~RpcStub() {
    zookeeper_close(m_zh);
    close(m_fd);
}

string RpcStub::call(const string &method_name, string request_data) {
    uint32_t service_name_length = m_service_name.size();
    uint32_t method_name_length = method_name.size();
    uint32_t req_length = request_data.size();

    string request;
    request.reserve(256);
    request.append(reinterpret_cast<char *>(&service_name_length), 4);
    request.append(m_service_name);
    request.append(reinterpret_cast<char *>(&method_name_length), 4);
    request.append(method_name);
    request.append(reinterpret_cast<char *>(&req_length), 4);
    request.append(move(request_data));
    send(m_fd, request.data(), request.size(), 0);

    string response(256, '\0');
    recv(m_fd, response.data(), response.size(), 0);

    StatusCode resp_state_code;
    memcpy(&resp_state_code, response.data(), 4);
    if (resp_state_code == NOT_FOUND_SERVICE) {
        cout << "not found service" << endl;
        return "";
    } else if (resp_state_code == NOT_FOUND_HANDLER) {
        cout << "not found handler" << endl;
        return "";
    }
    uint32_t resp_length;
    memcpy(&resp_length, response.data() + 4, 4);
    string response_data = response.substr(8, resp_length);
    return response_data;
}

future<AddResponse> CalcServiceStub::add(AddRequest req) {
    return async(launch::async, [this, req = move(req)] mutable {
        string request_data;
        req.SerializeToString(&request_data);

        string response_data = call("Add", move(request_data));
        AddResponse resp;
        resp.ParseFromString(response_data);
        return resp;
    });
}

future<SubtractResponse> CalcServiceStub::subtract(SubtractRequest req) {
    return async(launch::async, [this, req = move(req)] mutable {
        string request_data;
        req.SerializeToString(&request_data);

        string response_data = call("Subtract", move(request_data));
        SubtractResponse resp;
        resp.ParseFromString(response_data);
        return resp;
    });
}

future<EchoResponse> EchoServiceStub::echo(EchoRequest req) {
    return async(launch::async, [this, req = move(req)] mutable {
        string request_data;
        req.SerializeToString(&request_data);

        string response_data = call("Echo", move(request_data));
        EchoResponse resp;
        resp.ParseFromString(response_data);
        return resp;
    });
}