#pragma once

#include "message.pb.h"
#include <arpa/inet.h>
#include <atomic>
#include <future>
#include <string>
#include <vector>
#include <zookeeper/zookeeper.h>
using namespace std;

class RpcStub {
  public:
    RpcStub(const std::string &service_name);

  protected:
    std::string call(const std::string &handler_name, std::string request_data);

  protected:
    std::atomic<int> m_next_key_idx;
    std::string m_service_name;
    std::vector<std::string> m_keys;
};

class CalcService_Stub : public RpcStub {
  public:
    CalcService_Stub() : RpcStub("CalcService") {}
    AddResponse add(AddRequest req);
    SubtractResponse subtract(SubtractRequest req);
};

class EchoService_Stub : public RpcStub {
  public:
    EchoService_Stub() : RpcStub("EchoService") {}
    EchoResponse echo(EchoRequest req);
};