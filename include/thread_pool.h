#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <sys/eventfd.h>
#include <thread>
#include <vector>

class Task;

class ThreadPool {
  public:
    struct TaskResult {
        int fd;
        std::string response_data;
    };

    ThreadPool(int notify_fd, size_t n = 5);
    ~ThreadPool();
    void enqueue(std::unique_ptr<Task> task);
    void takeResults(std::queue<TaskResult> &q);

  private:
    void worker();

  private:
    int m_notify_fd;
    std::atomic<bool> m_stop = false;
    std::mutex m_mutex;
    std::mutex m_ready_mutex;
    std::condition_variable m_cond;
    std::queue<std::unique_ptr<Task>> m_task_queue;
    std::queue<TaskResult> m_ready_queue;
    std::vector<std::thread> m_workers;
};