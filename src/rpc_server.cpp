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
    close(m_conn_fd);
    close(m_listenfd);
}

void RpcServer::start() {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    m_conn_fd = accept(m_listenfd, reinterpret_cast<sockaddr *>(&client_addr), &client_len);

    while (true) {
        string request(256, '\0');
        recv(m_conn_fd, request.data(), 256, 0);

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
}

void RpcServer::registerService(const string &service_name, const string &handler_name, const function<string(const string &)> &handler) {
    m_handlers[service_name][handler_name] = handler;
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