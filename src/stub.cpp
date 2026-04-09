#include "stub.h"
#include "rpc_conn_pool.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
using namespace std;

RpcStub::RpcStub(const string &service_name) {
    m_service_name = service_name;

    zoo_set_debug_level(ZOO_LOG_LEVEL_ERROR);
    m_zh = zookeeper_init("127.0.0.1:2181", nullptr, 5000, nullptr, nullptr, 0);

    string path;
    path.reserve(16);
    path += "/TinyRPC/";
    path += service_name;
    char buf[64];
    int buf_len = 64;
    int ret = zoo_get(m_zh, path.data(), 0, buf, &buf_len, nullptr);
    if (ret == ZOK) {
        m_key.append(buf, buf_len);
    } else {
        cout << "get failed, code: " << ret << endl;
        return;
    }
}

RpcStub::~RpcStub() {
    zookeeper_close(m_zh);
}

string RpcStub::call(const string &method_name, string request_data) {
    string response;
    {
        ConnGuard conn_guard = conn_pool.getConnGuard(m_key);
        int fd = conn_guard.getConnFd();

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
        send(fd, request.data(), request.size(), 0);

        char buf[256];
        int n = recv(fd, buf, sizeof(buf), 0);
        response.append(buf, n);
    }

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

future<AddResponse> CalcService_Stub::add(AddRequest req) {
    return async(launch::async, [this, req = move(req)] mutable {
        string request_data;
        req.SerializeToString(&request_data);

        string response_data = call("Add", move(request_data));
        AddResponse resp;
        resp.ParseFromString(response_data);
        return resp;
    });
}

future<SubtractResponse> CalcService_Stub::subtract(SubtractRequest req) {
    return async(launch::async, [this, req = move(req)] mutable {
        string request_data;
        req.SerializeToString(&request_data);

        string response_data = call("Subtract", move(request_data));
        SubtractResponse resp;
        resp.ParseFromString(response_data);
        return resp;
    });
}

future<EchoResponse> EchoService_Stub::echo(EchoRequest req) {
    return async(launch::async, [this, req = move(req)] mutable {
        string request_data;
        req.SerializeToString(&request_data);

        string response_data = call("Echo", move(request_data));
        EchoResponse resp;
        resp.ParseFromString(response_data);
        return resp;
    });
}