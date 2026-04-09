#include "rpc_server.h"
#include "logger.h"
#include "message.pb.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
using namespace std;

RpcServer::RpcServer() {
    m_epoll_fd = epoll_create(1);

    string ip = "127.0.0.1";
    int port = 9090;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.data(), &addr.sin_addr);

    m_listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(m_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    bind(m_listen_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
    listen(m_listen_fd, 5);

    addfd(m_listen_fd);
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

    for (auto it = m_conns.begin(); it != m_conns.end(); it++) {
        close(it->second->fd);
    }
    close(m_listen_fd);
    close(m_epoll_fd);
}

void RpcServer::start() {
    LOG_INFO("\nserver start");
    const int max_events = 4096;
    epoll_event events[max_events];
    while (m_running) {
        int number = epoll_wait(m_epoll_fd, events, max_events, -1);
        if (number < 0) {
            if (errno == EINTR && m_running == false) {
                LOG_INFO("server stopped by signal SIGINT or SIGTERM");
            } else {
                LOG_ERROR("epoll_wait failed");
            }
            continue;
        } else if (number == 0) {
            LOG_INFO("epoll_wait return 0");
            continue;
        }

        for (int i = 0; i < number; i++) {
            int fd = events[i].data.fd;
            LOG_DEBUG("event fd = " + (fd == m_listen_fd ? "listen_fd" : to_string(fd)));
            if (fd == m_listen_fd) {
                while (true) {
                    sockaddr_in client_addr{};
                    socklen_t client_len = sizeof(client_addr);
                    int conn_fd = accept(m_listen_fd, reinterpret_cast<sockaddr *>(&client_addr), &client_len);
                    if (conn_fd <= 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }
                        LOG_ERROR("accept failed");
                        break;
                    }

                    addfd(conn_fd);
                    m_conns.emplace(conn_fd, make_unique<Connection>(conn_fd));

                    IF_DEBUG {
                        int client_port = ntohs(client_addr.sin_port);
                        char client_ip[32];
                        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

                        string msg;
                        msg.reserve(32);
                        msg += "new connection, fd = ";
                        msg += to_string(conn_fd);
                        msg += ", ip = ";
                        msg += client_ip;
                        msg += ':';
                        msg += to_string(client_port);
                        LOG_DEBUG(msg);
                    }
                }
            } else if (events[i].events & EPOLLIN) {
                const int buf_size = 256;
                m_conns[fd]->request.reserve(buf_size);
                char buf[buf_size];
                while (true) {
                    int n = recv(fd, buf, buf_size, 0);
                    if (n > 0) {
                        m_conns[fd]->request.append(buf, n);
                    } else if (n < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            LOG_DEBUG("fd = " + to_string(fd) + ", read will block, process and read next time");
                            if (!m_conns[fd]->request.empty()) {
                                LOG_DEBUG("fd = " + to_string(fd) + ", read data: " + m_conns[fd]->request);
                                process(*m_conns[fd]);
                            }
                            break;
                        }
                        LOG_ERROR("read failed");
                        break;
                    } else if (n == 0) {
                        LOG_DEBUG("fd = " + to_string(fd) + ", close writed");
                        if (!m_conns[fd]->request.empty()) {
                            LOG_DEBUG("fd = " + to_string(fd) + ", read data: " + m_conns[fd]->request);
                            process(*m_conns[fd]);
                        }
                        closeNow(fd);
                        break;
                    }
                }
            } else if (events[i].events & EPOLLOUT) {
                if (!m_conns[fd]->response_data.empty()) {
                    sendResponse(*m_conns[fd]);
                }
            } else {
                LOG_WARN("something else happened");
            }
        }
    }
}

void RpcServer::registerService(const string &service_name, const string &handler_name, const function<string(const string &)> &handler) {
    m_handlers[service_name][handler_name] = handler;
}

void RpcServer::registerToZk() {
    zoo_set_debug_level(ZOO_LOG_LEVEL_ERROR);
    zhandle_t *zh = zookeeper_init("127.0.0.1:2181", nullptr, 5000, nullptr, nullptr, 0);
    if (zh == nullptr) {
        LOG_ERROR("connect failed");
        return;
    }
    m_zh = zh;

    char path_buffer[64];
    int ret = zoo_create(zh, "/TinyRPC", "", 0, &ZOO_OPEN_ACL_UNSAFE, 0, path_buffer, sizeof(path_buffer));
    if (ret == ZOK) {
        LOG_DEBUG("create: " + string(path_buffer));
    } else {
        LOG_ERROR("create failed or already exists, code: " + to_string(ret));
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
            LOG_DEBUG("create: " + string(path_buffer));
        } else {
            LOG_ERROR("create failed or already exists, code: " + to_string(ret));
        }
        for (auto it2 = it1->second.begin(); it2 != it1->second.end(); it2++) {
            string handler_path = path;
            handler_path += '/';
            handler_path += it2->first;
            int ret = zoo_create(zh, handler_path.data(), addr.data(), addr.size(), &ZOO_OPEN_ACL_UNSAFE, 0, path_buffer, sizeof(path_buffer));
            if (ret == ZOK) {
                LOG_DEBUG("create: " + string(path_buffer));
            } else {
                LOG_ERROR("create failed or already exists, code: " + to_string(ret));
            }
        }
    }
}

void RpcServer::sendResponse(Connection &conn) {
    string response;
    response.reserve(8 + conn.response_data.size());

    uint32_t resp_state_code = static_cast<uint32_t>(conn.code);
    uint32_t resp_length = static_cast<uint32_t>(conn.response_data.size());

    response.append(reinterpret_cast<char *>(&resp_state_code), 4);
    response.append(reinterpret_cast<char *>(&resp_length), 4);
    response.append(move(conn.response_data));

    ssize_t sent = 0;
    while (sent < response.size()) {
        ssize_t n = send(conn.fd, response.data() + sent, response.size() - sent, 0);
        if (n > 0) {
            sent += n;
        } else if (n <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                LOG_DEBUG("send to fd = " + to_string(conn.fd) + "will block, send next time");
                modfd(conn.fd, EPOLLET | EPOLLIN | EPOLLOUT);
                break;
            }
            LOG_ERROR("read failed");
            break;
        }
    }
    LOG_DEBUG("send success");
    conn.response_data.clear();
};

static void setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
}

void RpcServer::addfd(int fd) {
    epoll_event event;
    event.events = EPOLLET | EPOLLIN;
    event.data.fd = fd;
    epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void RpcServer::modfd(int fd, uint32_t events) {
    epoll_event event;
    event.events = events;
    event.data.fd = fd;
    epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, fd, &event);
}

void RpcServer::process(Connection &conn) {
    uint32_t prefix_length = 0;
    auto read_prefixed_length_string = [&] {
        uint32_t length;
        memcpy(&length, conn.request.data() + prefix_length, 4);
        prefix_length += 4;
        string data = conn.request.substr(prefix_length, length);
        prefix_length += length;
        return data;
    };

    string service_name = read_prefixed_length_string();
    string handler_name = read_prefixed_length_string();
    string request_data = read_prefixed_length_string();
    conn.request.erase(0, prefix_length);

    auto it1 = m_handlers.find(service_name);
    if (it1 == m_handlers.end()) {
        cout << "no service" << endl;
        conn.code = NOT_FOUND_SERVICE;
        sendResponse(conn);
        return;
    }
    auto it2 = it1->second.find(handler_name);
    if (it2 == it1->second.end()) {
        cout << "no handler" << endl;
        conn.code = NOT_FOUND_HANDLER;
        sendResponse(conn);
        return;
    }
    conn.response_data = it2->second(request_data);
    conn.code = OK;
    sendResponse(conn);
}

void RpcServer::closeNow(int fd) {
    close(fd);
    m_conns.erase(fd);
}