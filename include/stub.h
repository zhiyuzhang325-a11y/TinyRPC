#pragma once

#include "message.pb.h"
#include "status_code.h"
#include <string>
#include <zookeeper/zookeeper.h>

class CalcServiceStub {
  public:
    CalcServiceStub();
    ~CalcServiceStub();
    AddResponse add(AddRequest req);
    SubtractResponse subtract(SubtractRequest req);

  private:
    std::string call(const std::string &service_name, const std::string &handler_name, std::string request_data);

  private:
    int m_fd;
    zhandle_t *m_zh;
};

class EchoServiceStub {
  public:
    EchoServiceStub();
    ~EchoServiceStub();
    EchoResponse echo(EchoRequest req);

  private:
    std::string call(const std::string &service_name, const std::string &method_name, std::string request_data);

  private:
    int m_fd;
    zhandle_t *m_zh;
};