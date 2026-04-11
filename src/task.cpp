#include "task.h"
using namespace std;

Task::Task(int fd, string request_data, const function<string(const string &)> &handler) : m_fd(fd), m_request_data(move(request_data)), m_handler(handler) {
    m_response_data.reserve(256);
}

void Task::handle() {
    m_response_data = m_handler(m_request_data);
}