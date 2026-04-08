#pragma once

#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>

class RpcConnPool;

class ConnGuard {
  public:
    ConnGuard(RpcConnPool &conn_pool, std::queue<int> *ptr = nullptr, int fd = -1) : m_fd(fd), m_queue_ptr(ptr), m_conn_pool(conn_pool) {}
    ~ConnGuard();
    int getConnFd() {
        return m_fd;
    }

  private:
    int m_fd;
    std::queue<int> *m_queue_ptr;
    RpcConnPool &m_conn_pool;
};

class RpcConnPool {
    friend class ConnGuard;

  public:
    RpcConnPool();
    ~RpcConnPool();
    ConnGuard getConnGuard(const std::string &key);

  private:
    std::unordered_map<std::string, std::unique_ptr<std::queue<int>>> m_pool;
    std::mutex m_mutex;
};

extern RpcConnPool conn_pool;