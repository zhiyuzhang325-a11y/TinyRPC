#pragma once

#include <string>

class RpcServer;

class CalcServiceImpl {
  public:
    CalcServiceImpl(RpcServer *rpc_server);
    std::string addHandler(const std::string &request_data);

  private:
    int add(int a, int b);

  private:
};