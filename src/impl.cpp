#include "impl.h"
#include "message.pb.h"
#include "rpc_server.h"
using namespace std;

CalcServiceImpl::CalcServiceImpl(RpcServer *rpc_server) {
    rpc_server->registerService("CalcService", "Add", [this](const string &request_data) {
        return this->addHandler(request_data);
    });
}

string CalcServiceImpl::addHandler(const string &request_data) {
    AddRequest req;
    req.ParseFromString(request_data);
    int a = req.a();
    int b = req.b();
    int c = add(a, b);

    string response_data;
    AddResponse resp;
    resp.set_c(c);
    resp.SerializeToString(&response_data);
    return response_data;
}

int CalcServiceImpl::add(int a, int b) {
    return a + b;
}