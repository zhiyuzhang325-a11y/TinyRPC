#include <iostream>
#include <string>
#include <zookeeper/zookeeper.h>
using namespace std;

int main() {
    zhandle_t *zh = zookeeper_init("127.0.0.1:2181", nullptr, 5000, nullptr, nullptr, 0);
    if (zh == nullptr) {
        cout << "connect failed" << endl;
        return -1;
    }
    cout << "connect success" << endl;

    string path_buffer(64, '\0');
    int ret = zoo_create(zh, "/test_node", "hello", 5, &ZOO_OPEN_ACL_UNSAFE, 0, path_buffer.data(), path_buffer.size());
    if (ret == ZOK) {
        cout << "create: " << path_buffer << endl;
    } else {
        cout << "create failed or already exists, code: " << ret << endl;
    }
    path_buffer.assign(64, '\0');
    ret = zoo_create(zh, "/test_node/node", "Hi", 2, &ZOO_OPEN_ACL_UNSAFE, 0, path_buffer.data(), path_buffer.size());
    if (ret == ZOK) {
        cout << "create: " << path_buffer << endl;
    } else {
        cout << "create failed or already exists, code: " << ret << endl;
    }

    string buf(64, '\0');
    int buf_len = buf.size();
    ret = zoo_get(zh, "/test_node", 0, buf.data(), &buf_len, nullptr);
    if (ret == ZOK) {
        cout << "data: " << buf << endl;
    } else {
        cout << "get failed, code: " << ret << endl;
    }

    zoo_delete(zh, "/test_node/node", -1);
    zoo_delete(zh, "/test_node", -1);

    zookeeper_close(zh);
}