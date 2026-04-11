#include "stub.h"
#include "rpc_conn_pool.h"
#include "status_code.h"
#include <arpa/inet.h>
#include <ctime>
#include <netinet/in.h>
#include <sys/socket.h>
using namespace std;

RpcStub::RpcStub(const string &service_name) {
    m_service_name = service_name;

    zoo_set_debug_level(ZOO_LOG_LEVEL_ERROR);
    zhandle_t *zh = zookeeper_init("127.0.0.1:2181", nullptr, 5000, nullptr, nullptr, 0);

    string path;
    path.reserve(16);
    path += "/TinyRPC/";
    path += service_name;
    String_vector children;
    int ret = zoo_get_children(zh, path.data(), 0, &children);
    if (ret != ZOK) {
        cout << "get_children failed, code: " << ret << endl;
        return;
    }

    for (int i = 0; i < children.count; i++) {
        string children_path = path + '/' + children.data[i];
        char buf[64];
        int buf_len = 64;
        int ret = zoo_get(zh, children_path.data(), 0, buf, &buf_len, nullptr);
        if (ret == ZOK) {
            m_keys.emplace_back(buf, buf_len);
        } else {
            cout << "get failed, code: " << ret << endl;
            return;
        }
    }
    deallocate_String_vector(&children);
    zookeeper_close(zh);

    srand(time(nullptr));
    m_next_key_idx = rand() % m_keys.size();
}

string RpcStub::call(const string &method_name, string request_data) {
    if (m_keys.empty()) {
        cout << "keys empty, can't get fd" << endl;
        return "";
    }

    uint32_t service_name_length = m_service_name.size();
    uint32_t method_name_length = method_name.size();
    uint32_t req_length = request_data.size();
    uint32_t total_length = 12 + service_name_length + method_name_length + req_length;

    string request;
    request.reserve(256);
    request.append(reinterpret_cast<const char *>(&MAGIC_NUMBER), 4);
    request.append(reinterpret_cast<char *>(&total_length), 4);
    request.append(reinterpret_cast<char *>(&service_name_length), 4);
    request.append(m_service_name);
    request.append(reinterpret_cast<char *>(&method_name_length), 4);
    request.append(method_name);
    request.append(reinterpret_cast<char *>(&req_length), 4);
    request.append(move(request_data));

    string response;
    {
        size_t idx = m_next_key_idx.fetch_add(1) % m_keys.size();
        cout << "idx=" << idx << " key=" << m_keys[idx] << endl;
        ConnGuard conn_guard = conn_pool.getConnGuard(m_keys[idx]);

        int fd = conn_guard.getConnFd();
        int ret = send(fd, request.data(), request.size(), 0);
        if (ret <= 0) {
            conn_guard.markBroken();
            return "";
        }

        char buf[256];
        int n = recv(fd, buf, sizeof(buf), 0);
        response.append(buf, n);
        if (n <= 0) {
            conn_guard.markBroken();
            return "";
        }
    }

    StatusCode resp_state_code;
    memcpy(&resp_state_code, response.data(), 4);
    if (resp_state_code == NOT_FOUND_SERVICE) {
        cout << "not found service" << endl;
        return "";
    } else if (resp_state_code == NOT_FOUND_HANDLER) {
        cout << "not found handler" << endl;
        return "";
    } else if (resp_state_code == INVAILD_REQUEST) {
        cout << "invaild request" << endl;
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