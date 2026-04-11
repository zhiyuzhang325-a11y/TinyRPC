#pragma once

#include "message.pb.h"
#include <arpa/inet.h>
#include <future>
#include <string>
#include <zookeeper/zookeeper.h>
using namespace std;

class RpcStub {
  public:
    RpcStub(const std::string &service_name);
    ~RpcStub();

  protected:
    std::string call(const std::string &handler_name, std::string request_data);

  protected:
    zhandle_t *m_zh;
    std::string m_service_name;
    std::string m_key;
};

class CalcService_Stub : public RpcStub {
  public:
    CalcService_Stub() : RpcStub("CalcService") {}
    std::future<AddResponse> add(AddRequest req);
    std::future<SubtractResponse> subtract(SubtractRequest req);
};

class EchoService_Stub : public RpcStub {
  public:
    EchoService_Stub() : RpcStub("EchoService") {}
    std::future<EchoResponse> echo(EchoRequest req);
};