#include "rpc_server.h"
#include "message.pb.h"
#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
using namespace std;

RpcServer::RpcServer() {
    string ip = "127.0.0.1";
    int port = 9090;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.data(), &addr.sin_addr);

    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    bind(m_listenfd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
    listen(m_listenfd, 5);
}

RpcServer::~RpcServer() {
    for (auto it1 = m_handlers.begin(); it1 != m_handlers.end(); it1++) {
        string path;
        path.reserve(64);
        path += "/TinyRPC";
        path += '/';
        path += it1->first;
        for (auto it2 = it1->second.begin(); it2 != it1->second.end(); it2++) {
            string handler_path = path;
            handler_path += '/';
            handler_path += it2->first;
            string addr = "127.0.0.1:9090";
            zoo_delete(m_zh, handler_path.data(), -1);
        }
        zoo_delete(m_zh, path.data(), -1);
    }
    int ret = zoo_delete(m_zh, "/TinyRPC", -1);
    zookeeper_close(m_zh);

    close(m_listenfd);
}

void RpcServer::start() {
    while (m_running) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        m_conn_fd = accept(m_listenfd, reinterpret_cast<sockaddr *>(&client_addr), &client_len);

        while (true) {
            string request(256, '\0');
            int n = recv(m_conn_fd, request.data(), 256, 0);
            if (n <= 0) break;

            uint32_t prefix_length = 0;
            auto read_prefixed_length_string = [&prefix_length, &request] {
                uint32_t length;
                memcpy(&length, request.data() + prefix_length, 4);
                prefix_length += 4;
                string data = request.substr(prefix_length, length);
                prefix_length += length;
                return data;
            };

            string service_name = read_prefixed_length_string();
            string handler_name = read_prefixed_length_string();
            string request_data = read_prefixed_length_string();

            auto it1 = m_handlers.find(service_name);
            if (it1 == m_handlers.end()) {
                cout << "no service" << endl;
                sendResponse(NOT_FOUND_SERVICE);
                return;
            }
            auto it2 = it1->second.find(handler_name);
            if (it2 == it1->second.end()) {
                cout << "no handler" << endl;
                sendResponse(NOT_FOUND_HANDLER);
                return;
            }
            string response_data = it2->second(request_data);
            sendResponse(OK, move(response_data));
        }
        close(m_conn_fd);
    }
}

void RpcServer::registerService(const string &service_name, const string &handler_name, const function<string(const string &)> &handler) {
    m_handlers[service_name][handler_name] = handler;
}

void RpcServer::registerToZk() {
    zhandle_t *zh = zookeeper_init("127.0.0.1:2181", nullptr, 5000, nullptr, nullptr, 0);
    if (zh == nullptr) {
        cout << "connect failed" << endl;
        return;
    }
    m_zh = zh;

    char path_buffer[64];
    int ret = zoo_create(zh, "/TinyRPC", "", 0, &ZOO_OPEN_ACL_UNSAFE, 0, path_buffer, sizeof(path_buffer));
    if (ret == ZOK) {
        cout << "create: " << path_buffer << endl;
    } else {
        cout << "create failed or already exists, code: " << ret << endl;
    }
    for (auto it1 = m_handlers.begin(); it1 != m_handlers.end(); it1++) {
        string path;
        path.reserve(64);
        path += "/TinyRPC";
        path += '/';
        path += it1->first;
        string addr = "127.0.0.1:9090";
        int ret = zoo_create(zh, path.data(), addr.data(), addr.size(), &ZOO_OPEN_ACL_UNSAFE, 0, path_buffer, sizeof(path_buffer));
        if (ret == ZOK) {
            cout << "create: " << path_buffer << endl;
        } else {
            cout << "create failed or already exists, code: " << ret << endl;
        }
        for (auto it2 = it1->second.begin(); it2 != it1->second.end(); it2++) {
            string handler_path = path;
            handler_path += '/';
            handler_path += it2->first;
            int ret = zoo_create(zh, handler_path.data(), addr.data(), addr.size(), &ZOO_OPEN_ACL_UNSAFE, 0, path_buffer, sizeof(path_buffer));
            if (ret == ZOK) {
                cout << "create: " << path_buffer << endl;
            } else {
                cout << "create failed or already exists, code: " << ret << endl;
            }
        }
    }
}

void RpcServer::sendResponse(StatusCode code, string response_data) {
    string response;
    response.reserve(8 + response_data.size());

    uint32_t resp_state_code = static_cast<uint32_t>(code);
    uint32_t resp_length = static_cast<uint32_t>(response_data.size());

    response.append(reinterpret_cast<char *>(&resp_state_code), 4);
    response.append(reinterpret_cast<char *>(&resp_length), 4);
    response.append(move(response_data));

    send(m_conn_fd, response.data(), response.size(), 0);
};