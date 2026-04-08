#pragma once

#include "status_code.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <zookeeper/zookeeper.h>

class RpcServer {
  private:
    struct Connection;

  public:
    RpcServer();
    ~RpcServer();
    void start();
    void registerService(const std::string &service_name, const std::string &handler_name, const std::function<std::string(const std::string &)> &handler);
    void registerToZk();
    void stop() {
        m_running = false;
    }

  private:
    void sendResponse(Connection &conn);
    void addfd(int fd);
    void modfd(int fd, uint32_t events);
    void process(Connection &conn);
    void closeNow(int fd);

  private:
    int m_epoll_fd;
    int m_listen_fd;
    zhandle_t *m_zh;
    std::unordered_map<std::string, std::unordered_map<std::string, std::function<std::string(const std::string &)>>> m_handlers;
    volatile bool m_running = true;

    struct Connection {
        int fd;
        StatusCode code;
        std::string request;
        std::string response_data;
    };
    std::unordered_map<int, std::unique_ptr<Connection>> m_conns;
};