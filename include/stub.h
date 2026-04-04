#pragma once

#include "message.pb.h"
#include "status_code.h"

class AddServiceStub {
  public:
    AddServiceStub(int fd);
    AddResponse add(AddRequest req);

  private:
    std::string call(const std::string &service_name, const std::string &handler_name, std::string request_data);

  private:
    int m_fd;
};