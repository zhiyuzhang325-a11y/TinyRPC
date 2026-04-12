#pragma once

#include "service_impl.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <zookeeper/zookeeper.h>

using HandlerMap = std::unordered_map<std::string, std::unordered_map<std::string, std::function<std::string(const std::string &)>>>;

class SubReactor;

class RpcServer {
  public:
    RpcServer(const std::string &ip = "127.0.0.1", int port = 9090, int n = 4, int workers = 4);
    ~RpcServer();
    void registerService(const std::string &service_name, const std::string &handler_name, const std::function<std::string(const std::string &)> &handler);
    void registerToZk();
    void start();
    void shutDown();

  private:
    void addfd(int fd);

  private:
    int m_listen_fd;
    int m_epoll_fd;
    const int m_num_reactors;
    volatile bool m_running = true;
    zhandle_t *m_zh;
    std::string m_addr;
    std::shared_ptr<HandlerMap> m_handlers = std::make_shared<HandlerMap>();
    std::vector<std::unique_ptr<SubReactor>> m_sub_reactors;
    CalcServiceImpl m_calc_impl;
    EchoServiceImpl m_echo_impl;
};