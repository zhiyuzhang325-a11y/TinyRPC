#pragma once

#include "message.pb.h"
#include "status_code.h"
#include <future>
#include <string>
#include <zookeeper/zookeeper.h>

class RpcStub {
  public:
    RpcStub(const std::string &service_name);
    ~RpcStub();

  protected:
    std::string call(const std::string &handler_name, std::string request_data);
    int m_fd;
    zhandle_t *m_zh;
    std::string m_service_name;
};

class CalcServiceStub : public RpcStub {
  public:
    CalcServiceStub() : RpcStub("CalcService") {}
    std::future<AddResponse> add(AddRequest req);
    std::future<SubtractResponse> subtract(SubtractRequest req);
};

class EchoServiceStub : public RpcStub {
  public:
    EchoServiceStub() : RpcStub("EchoService") {}
    std::future<EchoResponse> echo(EchoRequest req);
};