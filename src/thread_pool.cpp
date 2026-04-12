#include "thread_pool.h"
#include "task.h"
#include <unistd.h>
using namespace std;

ThreadPool::ThreadPool(int notify_fd, size_t n) : m_notify_fd(notify_fd) {
    for (int i = 0; i < n; i++) {
        m_workers.emplace_back(&ThreadPool::worker, this);
    }
}

ThreadPool::~ThreadPool() {
    m_stop = true;
    m_cond.notify_all();

    for (auto &t : m_workers) {
        if (t.joinable()) {
            t.join();
        }
    }
}

void ThreadPool::enqueue(unique_ptr<Task> task) {
    lock_guard<mutex> lock(m_mutex);
    m_task_queue.emplace(move(task));
    m_cond.notify_one();
}

void ThreadPool::takeResults(queue<ThreadPool::TaskResult> &q) {
    {
        lock_guard<mutex> lock(m_ready_mutex);
        q.swap(m_ready_queue);
    }
}

void ThreadPool::worker() {
    while (true) {
        unique_ptr<Task> task;
        {
            unique_lock<mutex> lock(m_mutex);
            m_cond.wait(lock, [this] {
                return !m_task_queue.empty() || m_stop;
            });

            if (m_stop && m_task_queue.empty()) {
                return;
            }

            task = move(m_task_queue.front());
            m_task_queue.pop();
        }
        if (task == nullptr) continue;

        task->handle();
        {
            lock_guard<mutex> lock(m_ready_mutex);
            m_ready_queue.emplace(task->m_fd, move(task->m_response_data));
        }

        uint64_t val = 1;
        write(m_notify_fd, &val, sizeof(val));
    }
}