#pragma once

#include "status_code.h"
#include <functional>
#include <string>
#include <unordered_map>

class RpcServer {
  public:
    RpcServer();
    ~RpcServer();
    void start();
    void registerService(const std::string &service_name, const std::string &handler_name, const std::function<std::string(const std::string &)> &handler);

  private:
    void sendResponse(StatusCode state, std::string response_data = "");

  private:
    int m_listenfd;
    int m_conn_fd;
    std::unordered_map<std::string, std::unordered_map<std::string, std::function<std::string(const std::string &)>>> m_handlers;
    std::unordered_map<uint32_t, uint32_t> m_type_cast;
};