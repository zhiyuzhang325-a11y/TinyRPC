#pragma once

#include <functional>
#include <mutex>
#include <queue>
#include <string>

class Task {
    friend class ThreadPool;

  public:
    Task(int fd, std::string request_data, const std::function<std::string(const std::string &)> &handler);
    void handle();

  private:
    int m_fd;
    std::string m_request_data;
    std::string m_response_data;
    const std::function<std::string(const std::string &)> &m_handler;
};