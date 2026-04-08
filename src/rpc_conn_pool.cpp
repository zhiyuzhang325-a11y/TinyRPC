#include "rpc_conn_pool.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using namespace std;

RpcConnPool conn_pool;

RpcConnPool::RpcConnPool() {}

RpcConnPool::~RpcConnPool() {
    for (auto it = m_pool.begin(); it != m_pool.end(); it++) {
        auto &fd_queue = it->second;
        while (!fd_queue->empty()) {
            close(fd_queue->front());
            fd_queue->pop();
        }
    }
}

ConnGuard RpcConnPool::getConnGuard(const string &key) {
    size_t pos = key.find(':');
    if (pos == string::npos) {
        return ConnGuard(*this);
    }
    string ip(key.substr(0, pos));
    int port = stoi(string(key.substr(pos + 1, key.size() - pos)));

    int fd = 0;
    queue<int> *ptr = nullptr;
    {
        lock_guard<mutex> lock(m_mutex);
        if (!m_pool.contains(key)) {
            m_pool.emplace(key, make_unique<queue<int>>());
        }
        ptr = m_pool[key].get();

        if (!m_pool[key]->empty()) {
            fd = m_pool[key]->front();
            m_pool[key]->pop();
        }
    }

    if (fd == 0) {
        fd = socket(PF_INET, SOCK_STREAM, 0);

        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, ip.data(), &addr.sin_addr);
        connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
    }
    return ConnGuard(*this, ptr, fd);
}

ConnGuard::~ConnGuard() {
    if (m_fd > 0 && m_queue_ptr != nullptr) {
        lock_guard<mutex> lock(m_conn_pool.m_mutex);
        m_queue_ptr->emplace(m_fd);
    }
}