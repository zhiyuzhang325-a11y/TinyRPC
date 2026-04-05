#include "impl.h"
#include "message.pb.h"
#include "rpc_server.h"
using namespace std;

CalcServiceImpl::CalcServiceImpl(RpcServer *rpc_server) {
    rpc_server->registerService("CalcService", "Add", [this](const string &request_data) {
        return this->addHandler(request_data);
    });
    rpc_server->registerService("CalcService", "Subtract", [this](const string &request_data) {
        return this->subtractHandler(request_data);
    });
}

string CalcServiceImpl::addHandler(const string &request_data) {
    AddRequest req;
    req.ParseFromString(request_data);
    int a = req.a();
    int b = req.b();
    int c = add(a, b);

    AddResponse resp;
    resp.set_c(c);
    string response_data;
    resp.SerializeToString(&response_data);
    return response_data;
}

int CalcServiceImpl::add(int a, int b) {
    return a + b;
}

string CalcServiceImpl::subtractHandler(const string &request_data) {
    SubtractRequest req;
    req.ParseFromString(request_data);
    int a = req.a();
    int b = req.b();
    int c = subtract(a, b);

    SubtractResponse resp;
    resp.set_c(c);
    string response_data;
    resp.SerializeToString(&response_data);
    return response_data;
}

int CalcServiceImpl::subtract(int a, int b) {
    return a - b;
}

EchoServiceImpl::EchoServiceImpl(RpcServer *rpc_server) {
    rpc_server->registerService("EchoService", "Echo", [this](const string &request_data) {
        return this->echoHandler(request_data);
    });
}

string EchoServiceImpl::echoHandler(const string &request_data) {
    EchoRequest req;
    req.ParseFromString(request_data);
    string request_msg = request_data;
    string response_msg = echo(request_msg);

    EchoResponse resp;
    resp.set_msg(response_msg);
    string response_data;
    resp.SerializeToString(&response_data);
    return response_data;
}

string EchoServiceImpl::echo(const string &msg) {
    return msg;
}