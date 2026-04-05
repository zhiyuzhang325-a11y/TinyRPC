#pragma once

#include <string>

class RpcServer;

class CalcServiceImpl {
  public:
    CalcServiceImpl(RpcServer *rpc_server);
    std::string addHandler(const std::string &request_data);
    std::string subtractHandler(const std::string &request_data);

  private:
    int add(int a, int b);
    int subtract(int a, int b);

  private:
};

class EchoServiceImpl {
  public:
    EchoServiceImpl(RpcServer *rpc_server);
    std::string echoHandler(const std::string &request);

  private:
    std::string echo(const std::string &msg);

  private:
};