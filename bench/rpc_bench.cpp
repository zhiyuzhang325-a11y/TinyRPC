#include "stub.h"
#include <atomic>
#include <ctime>
#include <future>
#include <thread>
using namespace std;

atomic<bool> running = true;
const int MOD = 1e8;

tuple<vector<int64_t>, int64_t, int64_t> syncBenchWorker() {
    vector<int64_t> latencies;
    int64_t success = 0, fail = 0;
    CalcService_Stub calc;
    EchoService_Stub echo;

    while (running) {
        int a = 1, b = 2;
        AddRequest add_req;
        add_req.set_a(a), add_req.set_b(b);

        auto start = chrono::steady_clock::now();
        auto add_resp = calc.add(add_req);
        auto end = chrono::steady_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        latencies.emplace_back(us);

        int c = add_resp.c();
        if (c == a + b) {
            success++;
        } else {
            fail++;
        }
    }

    return {latencies, success, fail};
}

int main(int argc, char *argv[]) {
    int num;
    if (argc > 1) {
        num = stoi(argv[1]);
    } else {
        num = 10;
    }

    vector<int64_t> all_latencies;
    int64_t total_success = 0, total_fail = 0;

    vector<future<tuple<vector<int64_t>, int64_t, int64_t>>> f;
    for (int i = 0; i < num; i++) {
        f.emplace_back(async(launch::async, syncBenchWorker));
        // f.emplace_back(async(launch::async, asyncBenchWorker, batch_size));
    }

    sleep(10);
    running = false;

    for (auto &ff : f) {
        auto [latencies, success, fail] = ff.get();
        all_latencies.insert(all_latencies.end(), latencies.begin(), latencies.end());
        total_success += success, total_fail += fail;
    }

    sort(all_latencies.begin(), all_latencies.end());
    int64_t n = all_latencies.size();
    int64_t p50 = all_latencies[n * 50 / 100];
    int64_t p99 = all_latencies[n * 99 / 100];
    int64_t p999 = all_latencies[n * 999 / 1000];

    int64_t seconds = 10;
    double qps = 1.0 * n / seconds;

    cout << "success rate: " << 1.0 * total_success / n << endl;
    cout << "QPS: " << qps << endl;
    cout << "p50: " << p50 << endl;
    cout << "p99: " << p99 << endl;
    cout << "p999: " << p999 << endl;
}

// g++ -std=c++20 bench/rpc_bench.cpp src/stub.cpp src/rpc_conn_pool.cpp build/message.pb.cc -o build/rpc_bench.out -Iinclude -Ibuild -lpthread -lprotobuf -lzookeeper_mt