#pragma once

#include "message.pb.h"
#include "status_code.h"

class CalcServiceStub {
  public:
    CalcServiceStub(int fd);
    AddResponse add(AddRequest req);
    SubtractResponse subtract(SubtractRequest req);

  private:
    std::string call(const std::string &service_name, const std::string &handler_name, std::string request_data);

  private:
    int m_fd;
};

class EchoServiceStub {
  public:
    EchoServiceStub(int fd);
    EchoResponse echo(EchoRequest req);

  private:
    std::string call(const std::string &service_name, const std::string &handler_name, std::string request_data);

  private:
    int m_fd;
};