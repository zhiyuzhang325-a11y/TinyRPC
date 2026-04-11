#pragma once

#include "rpc_type.h"
#include "status_code.h"
#include "thread_pool.h"
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>

class SubReactor {
  private:
    struct Connection;

  public:
    SubReactor(int i, std::shared_ptr<const HandlerMap> handlers);
    ~SubReactor();
    void start();
    void stop() {
        m_running = false;
        notifyDispatch();
    }
    void addConnection(int fd);

  private:
    void loop();
    void sendResponse(Connection &conn);
    void addfd(int fd);
    void modfd(int fd, uint32_t events);
    void process(Connection &conn);
    void closeNow(int fd);
    int32_t checkComplete(const std::string &request);
    void notifyDispatch();

  private:
    int m_idx;
    int m_epoll_fd;
    int m_ready_notify_fd;
    int m_dispatch_notify_fd;
    std::shared_ptr<const HandlerMap> m_handlers;
    std::atomic<bool> m_running = true;
    ThreadPool m_thread_pool;
    std::mutex m_conn_mutex;
    std::queue<int> m_conn_queue;
    std::thread m_loop_thread;

    struct Connection {
        int fd;
        StatusCode code;
        std::string request;
        std::string response_data;
    };
    std::unordered_map<int, std::unique_ptr<Connection>> m_conns;
};