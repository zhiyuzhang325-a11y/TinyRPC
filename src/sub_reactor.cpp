#include "sub_reactor.h"
#include "logger.h"
#include "message.pb.h"
#include "task.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
using namespace std;

static int eventfd_create() {
    int fd = eventfd(0, EFD_NONBLOCK);
    if (fd < 0) {
        LOG_ERROR("eventfd create failed");
        throw std::runtime_error("eventfd create failed");
    }
    return fd;
}

SubReactor::SubReactor(int i, std::shared_ptr<const HandlerMap> handlers, int workers) : m_idx(i), m_ready_notify_fd(eventfd_create()), m_handlers(handlers), m_thread_pool(m_ready_notify_fd, workers) {
    m_epoll_fd = epoll_create(1);
    m_dispatch_notify_fd = eventfd_create();
    addfd(m_ready_notify_fd);
    addfd(m_dispatch_notify_fd);
}

SubReactor::~SubReactor() {
    if (m_loop_thread.joinable()) {
        m_loop_thread.join();
    }

    for (auto it = m_conns.begin(); it != m_conns.end(); it++) {
        close(it->second->fd);
    }
    close(m_ready_notify_fd);
    close(m_dispatch_notify_fd);
    close(m_epoll_fd);
}

void SubReactor::addConnection(int fd) {
    {
        lock_guard<mutex> lock(m_conn_mutex);
        m_conn_queue.emplace(fd);
    }
    notifyDispatch();
}

void SubReactor::notifyDispatch() {
    uint64_t val = 1;
    write(m_dispatch_notify_fd, &val, sizeof(val));
}

static void setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
}

void SubReactor::addfd(int fd) {
    epoll_event event;
    event.events = EPOLLET | EPOLLIN;
    event.data.fd = fd;
    epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void SubReactor::modfd(int fd, uint32_t events) {
    epoll_event event;
    event.events = events;
    event.data.fd = fd;
    epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, fd, &event);
}

void SubReactor::start() {
    LOG_INFO("\nserver start");
    m_loop_thread = thread(&SubReactor::loop, this);
}

void SubReactor::loop() {
    const int max_events = 4096;
    epoll_event events[max_events];
    while (m_running) {
        int number = epoll_wait(m_epoll_fd, events, max_events, -1);
        if (number < 0) {
            if (errno == EINTR && m_running == false) {
                LOG_INFO("server stopped by signal SIGINT or SIGTERM");
            } else {
                LOG_ERROR("epoll_wait failed");
            }
            continue;
        } else if (number == 0) {
            LOG_INFO("epoll_wait return 0");
            continue;
        }

        for (int i = 0; i < number; i++) {
            int fd = events[i].data.fd;
            LOG_DEBUG("event fd = " + (fd == m_dispatch_notify_fd ? "dispatch_fd" : to_string(fd)));
            if (fd == m_ready_notify_fd) {
                uint64_t val;
                read(m_ready_notify_fd, &val, sizeof(val));

                queue<ThreadPool::TaskResult> ready_queue;
                m_thread_pool.takeResults(ready_queue);
                while (!ready_queue.empty()) {
                    auto &[conn_fd, response_data] = ready_queue.front();
                    LOG_DEBUG("fd = " + to_string(conn_fd) + ", start to sendResponse");
                    m_conns[conn_fd]->response_data = move(response_data);
                    m_conns[conn_fd]->code = OK;
                    sendResponse(*m_conns[conn_fd]);

                    ready_queue.pop();
                }
            } else if (fd == m_dispatch_notify_fd) {
                uint64_t val;
                read(m_dispatch_notify_fd, &val, sizeof(val));

                queue<int> conn_queue;
                {
                    lock_guard<mutex> lock(m_conn_mutex);
                    conn_queue.swap(m_conn_queue);
                }

                while (!conn_queue.empty()) {
                    int conn_fd = conn_queue.front();
                    conn_queue.pop();

                    addfd(conn_fd);
                    m_conns.emplace(conn_fd, make_unique<Connection>(conn_fd));
                }
            } else if (events[i].events & EPOLLIN) {
                const int buf_size = 256;
                m_conns[fd]->request.reserve(buf_size);
                char buf[buf_size];
                while (true) {
                    int n = recv(fd, buf, buf_size, 0);
                    if (n > 0) {
                        m_conns[fd]->request.append(buf, n);
                    } else if (n < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            LOG_DEBUG("fd = " + to_string(fd) + ", read will block, process and read next time");

                            int ret = 0;
                            while (ret = checkComplete(m_conns[fd]->request)) {
                                if (ret < 0) {
                                    LOG_DEBUG("fd = " + to_string(fd) + ", invaild packet, try resync");
                                    size_t next_start_pos = m_conns[fd]->request.find(reinterpret_cast<const char *>(&MAGIC_NUMBER), 4, 4);
                                    if (next_start_pos == string::npos) {
                                        LOG_DEBUG("clear request");
                                        m_conns[fd]->request.clear();
                                        break;
                                    } else {
                                        LOG_DEBUG("erase invaild packet");
                                        m_conns[fd]->request.erase(0, next_start_pos);
                                        continue;
                                    }
                                }
                                LOG_DEBUG("fd = " + to_string(fd) + ", read data: " + m_conns[fd]->request);
                                process(*m_conns[fd]);
                            }

                            break;
                        }
                        break;
                    } else if (n == 0) {
                        LOG_DEBUG("fd = " + to_string(fd) + ", close writed");

                        int ret = 0;
                        while (ret = checkComplete(m_conns[fd]->request)) {
                            if (ret < 0) {
                                LOG_DEBUG("fd = " + to_string(fd) + ", invaild packet, try resync");
                                size_t next_start_pos = m_conns[fd]->request.find(reinterpret_cast<const char *>(&MAGIC_NUMBER), 8, 4);
                                if (next_start_pos == string::npos) {
                                    LOG_DEBUG("clear request");
                                    m_conns[fd]->request.clear();
                                    break;
                                } else {
                                    LOG_DEBUG("erase invaild packet");
                                    m_conns[fd]->request.erase(0, next_start_pos);
                                    continue;
                                }
                            }
                            LOG_DEBUG("fd = " + to_string(fd) + ", read data: " + m_conns[fd]->request);
                            process(*m_conns[fd]);
                        }

                        closeNow(fd);
                        break;
                    }
                }
            } else if (events[i].events & EPOLLOUT) {
                if (!m_conns[fd]->response_data.empty()) {
                    sendResponse(*m_conns[fd]);
                }
            } else {
                LOG_WARN("something else happened");
            }
        }
    }
}

void SubReactor::process(Connection &conn) {
    uint32_t prefix_length = 8;
    auto read_prefixed_length_string = [&] {
        uint32_t length;
        memcpy(&length, conn.request.data() + prefix_length, 4);
        if (length > conn.request.size()) {
            conn.code = INVAILD_REQUEST;
            sendResponse(conn);
            return string();
        }
        LOG_DEBUG("prefix_length: " + to_string(prefix_length) + ", length: " + to_string(length));
        prefix_length += 4;
        string data = conn.request.substr(prefix_length, length);
        prefix_length += length;
        return data;
    };

    LOG_DEBUG("fd = " + to_string(conn.fd) + " request size: " + to_string(conn.request.size()) + ", prefix_length: " + to_string(prefix_length));
    string service_name = read_prefixed_length_string();
    if (service_name.empty()) return;
    string handler_name = read_prefixed_length_string();
    if (handler_name.empty()) return;
    string request_data = read_prefixed_length_string();
    if (request_data.empty()) return;
    conn.request.erase(0, prefix_length);

    auto it1 = m_handlers->find(service_name);
    if (it1 == m_handlers->end()) {
        cout << "no service" << endl;
        conn.code = NOT_FOUND_SERVICE;
        sendResponse(conn);
        return;
    }
    auto it2 = it1->second.find(handler_name);
    if (it2 == it1->second.end()) {
        cout << "no handler" << endl;
        conn.code = NOT_FOUND_HANDLER;
        sendResponse(conn);
        return;
    }

    LOG_DEBUG("fd = " + to_string(conn.fd) + " enqueue");
    unique_ptr<Task> task(make_unique<Task>(conn.fd, move(request_data), it2->second));
    m_thread_pool.enqueue(move(task));
}

void SubReactor::sendResponse(Connection &conn) {
    string response;
    response.reserve(8 + conn.response_data.size());

    uint32_t resp_state_code = static_cast<uint32_t>(conn.code);
    uint32_t resp_length = static_cast<uint32_t>(conn.response_data.size());

    response.append(reinterpret_cast<char *>(&resp_state_code), 4);
    response.append(reinterpret_cast<char *>(&resp_length), 4);
    response.append(move(conn.response_data));

    ssize_t sent = 0;
    while (sent < response.size()) {
        ssize_t n = send(conn.fd, response.data() + sent, response.size() - sent, 0);
        if (n > 0) {
            sent += n;
        } else if (n <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                LOG_DEBUG("send to fd = " + to_string(conn.fd) + "will block, send next time");
                modfd(conn.fd, EPOLLET | EPOLLIN | EPOLLOUT);
                break;
            }
            LOG_ERROR("read failed");
            break;
        }
    }
    LOG_DEBUG("send success");
    conn.response_data.clear();
};

int32_t SubReactor::checkComplete(const string &request) {
    if (request.size() < 8) {
        LOG_DEBUG("imcomplete packet: less then 8 bytes for magic or total_length");
        return 0;
    }

    int32_t magic;
    memcpy(&magic, request.data(), 4);
    if (magic != MAGIC_NUMBER) {
        return -1;
    }

    int32_t total_length;
    memcpy(&total_length, request.data() + 4, 4);
    if (request.size() < 8 + total_length) {
        LOG_DEBUG("imcomplete packet: wait for body");
        return 0;
    }
    return 8 + total_length;
}

void SubReactor::closeNow(int fd) {
    close(fd);
    m_conns.erase(fd);
}