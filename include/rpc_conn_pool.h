#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>

class RpcConnPool;

class ConnGuard {
  public:
    ConnGuard(RpcConnPool &conn_pool, const std::string &key = "", int fd = -1) : m_fd(fd), m_key(key), m_conn_pool(conn_pool) {}
    ~ConnGuard();
    int getConnFd() { return m_fd; }
    void markBroken();

  private:
    int m_fd;
    const std::string &m_key;
    RpcConnPool &m_conn_pool;
};

class RpcConnPool {
    friend class ConnGuard;

  public:
    RpcConnPool();
    ~RpcConnPool();
    ConnGuard getConnGuard(const std::string &key);

  private:
    int m_max_conns_per_entry = 1024;
    struct PoolEntry {
        std::queue<int> fds;
        int total_counts = 0;
    };
    std::unordered_map<std::string, PoolEntry> m_pool;
    std::mutex m_mutex;
    std::condition_variable m_cond;
};

extern RpcConnPool conn_pool;