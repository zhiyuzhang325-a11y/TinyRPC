#include "rpc_server.h"
#include "logger.h"
#include "sub_reactor.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>
using namespace std;

RpcServer::RpcServer(const string &ip, int port, int n, int workers) : m_num_reactors(n), m_addr(ip + ':' + to_string(port)), m_calc_impl(), m_echo_impl() {
    m_calc_impl.registerService(this);
    m_echo_impl.registerService(this);
    registerToZk();

    for (int i = 0; i < m_num_reactors; i++) {
        m_sub_reactors.emplace_back(make_unique<SubReactor>(i, m_handlers, workers));
        m_sub_reactors[i]->start();
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.data(), &addr.sin_addr);

    m_listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(m_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    bind(m_listen_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
    listen(m_listen_fd, 5);

    m_epoll_fd = epoll_create1(0);
    addfd(m_listen_fd);
}

RpcServer::~RpcServer() {
    for (auto &reactor : m_sub_reactors) {
        reactor->stop();
    }

    zookeeper_close(m_zh);
    close(m_listen_fd);
    close(m_epoll_fd);
}

void RpcServer::registerService(const string &service_name, const string &handler_name, const function<string(const string &)> &handler) {
    (*m_handlers)[service_name][handler_name] = handler;
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
    for (auto it1 = m_handlers->begin(); it1 != m_handlers->end(); it1++) {
        string path;
        path.reserve(64);
        path += "/TinyRPC";
        path += '/';
        path += it1->first;
        int ret = zoo_create(zh, path.data(), m_addr.data(), m_addr.size(), &ZOO_OPEN_ACL_UNSAFE, 0, path_buffer, sizeof(path_buffer));
        if (ret == ZOK) {
            LOG_DEBUG("create: " + string(path_buffer));
        } else {
            LOG_ERROR("create failed or already exists, code: " + to_string(ret));
        }

        path += "/node";
        ret = zoo_create(zh, path.data(), m_addr.data(), m_addr.size(), &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL | ZOO_SEQUENCE, path_buffer, sizeof(path_buffer));
        if (ret == ZOK) {
            LOG_DEBUG("create: " + string(path_buffer));
        } else {
            LOG_ERROR("create failed or already exists, code: " + to_string(ret));
        }
    }
}

void RpcServer::start() {
    int next_reactor_idx = 0;
    while (m_running) {
        int max_events = 4096;
        epoll_event events[max_events];

        int n = epoll_wait(m_epoll_fd, events, max_events, -1);
        if (n <= 0) {
            if (errno == EINTR) {
                LOG_INFO("server stopped by signal SIGINT or SIGTERM");
            } else {
                LOG_ERROR("epoll_wait failed");
            }
            continue;
        }

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;
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

                    IF_DEBUG {
                        int client_port = ntohs(client_addr.sin_port);
                        char client_ip[32];
                        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

                        string msg;
                        msg.reserve(64);
                        msg += "new connection, fd = ";
                        msg += to_string(conn_fd);
                        msg += ", ip = ";
                        msg += client_ip;
                        msg += ':';
                        msg += to_string(client_port);
                        msg += " dispatch to reactor[";
                        msg += to_string(next_reactor_idx);
                        msg += ']';
                        LOG_DEBUG(msg);
                    }

                    m_sub_reactors[next_reactor_idx]->addConnection(conn_fd);
                    next_reactor_idx = (next_reactor_idx + 1) % m_num_reactors;
                }
            } else {
                LOG_WARN("something else happened to mian reactor");
            }
        }
    }
}

void RpcServer::shutDown() {
    m_running = false;
}

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