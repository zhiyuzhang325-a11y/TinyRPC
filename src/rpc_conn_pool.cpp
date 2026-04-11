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
        auto &[fds, n] = it->second;
        while (!fds.empty()) {
            close(fds.front());
            fds.pop();
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
    {
        unique_lock<mutex> lock(m_mutex);
        if (!m_pool.contains(key)) {
            m_pool.emplace(key, PoolEntry{queue<int>{}, 0});
        }
        PoolEntry &entry = m_pool[key];

        while (!entry.fds.empty()) {
            fd = entry.fds.front();
            entry.fds.pop();

            char c;
            int ret = recv(fd, &c, 1, MSG_PEEK | MSG_DONTWAIT);
            if (ret == 0 || (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                close(fd);
                fd = 0;
                entry.total_counts--;
            } else {
                break;
            }
        }

        if (fd == 0) {
            m_cond.wait(lock, [&fd, &entry, this] {
                if (entry.total_counts < m_max_conns_per_entry) {
                    entry.total_counts++;
                    return true;
                } else if (!entry.fds.empty()) {
                    fd = entry.fds.front();
                    entry.fds.pop();
                    return true;
                }
                return false;
            });
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
    return ConnGuard(*this, key, fd);
}

ConnGuard::~ConnGuard() {
    if (m_fd != 0 && !m_key.empty()) {
        lock_guard<mutex> lock(m_conn_pool.m_mutex);
        if (m_fd > 0) {
            m_conn_pool.m_pool[m_key].fds.emplace(m_fd);
        } else {
            m_conn_pool.m_pool[m_key].total_counts--;
        }
        m_conn_pool.m_cond.notify_one();
    }
}

void ConnGuard::markBroken() {
    close(m_fd);
    m_fd = -1;
}