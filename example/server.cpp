#include "logger.h"
#include "rpc_server.h"
#include <signal.h>
#include <string>
#include <sys/wait.h>
using namespace std;

RpcServer *g_server = nullptr;

void handleSignal(int sig) {
    if (g_server) {
        LOG_INFO("server stop\n");
        g_server->shutDown();
        g_server = nullptr;
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    AsyncLogger::setLevel(AsyncLogger::ERROR);

    while (true) {
        pid_t pid = fork();
        if (pid > 0) {
            AsyncLogger::getInstance().setFilePath("logs/daemon.log");
            LOG_INFO("process start, pid is " + to_string(pid));

            int status;
            int ret = waitpid(pid, &status, 0);
            LOG_INFO("waitpid returned, ret=" + to_string(ret) + ", status=" + to_string(status));
            if (WIFEXITED(status)) {
                int code = WEXITSTATUS(status);
                LOG_INFO("\nserver stop by code: " + to_string(status));
                break;
            } else if (WIFSIGNALED(status)) {
                int sig = WTERMSIG(status);
                {
                    string msg;
                    msg.reserve(64);
                    msg += "pid = ";
                    msg += to_string(pid);
                    msg += ", process termintated by signal ";
                    msg += to_string(sig);
                    msg += ", restarting...";
                    LOG_INFO(msg);
                }
                continue;
            } else {
                LOG_INFO("unknown exit status: " + to_string(status));
                break;
            }
        } else if (pid < 0) {
            LOG_ERROR("fork failed");
            continue;
        } else {
            AsyncLogger::getInstance().setFilePath("logs/server.log");

            signal(SIGINT, handleSignal);
            signal(SIGTERM, handleSignal);

            string ip = "127.0.0.1";
            int port = 9090;
            if (argc > 2) {
                ip = argv[1];
                port = stoi(argv[2]);
            }
            cout << "RpcServer ip is " + ip + '/' << port << endl;
            int sub_n;
            int worker_n;
            if (argc > 4) {
                int sub_n = stoi(argv[3]);
                int worker_n = stoi(argv[4]);
            }
            cout << "sub: " << sub_n << " worker: " << worker_n << endl;
            RpcServer rpc_server(ip, port);
            g_server = &rpc_server;
            rpc_server.start();

            return 0;
        }
    }
}