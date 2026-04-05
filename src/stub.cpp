#include "stub.h"
#include <sys/socket.h>
using namespace std;

CalcServiceStub::CalcServiceStub(int fd) : m_fd(fd) {}

AddResponse CalcServiceStub::add(AddRequest req) {
    string request_data;
    req.SerializeToString(&request_data);

    string response_data = call("CalcService", "Add", move(request_data));
    AddResponse resp;
    resp.ParseFromString(response_data);
    return resp;
}

SubtractResponse CalcServiceStub::subtract(SubtractRequest req) {
    string request_data;
    req.SerializeToString(&request_data);

    string response_data = call("CalcService", "Subtract", request_data);
    SubtractResponse resp;
    resp.ParseFromString(response_data);
    return resp;
}

string CalcServiceStub::call(const string &service_name, const string &handler_name, string request_data) {
    uint32_t service_name_length = service_name.size();
    uint32_t handler_name_length = handler_name.size();
    uint32_t req_length = request_data.size();

    string request;
    request.reserve(256);
    request.append(reinterpret_cast<char *>(&service_name_length), 4);
    request.append(service_name);
    request.append(reinterpret_cast<char *>(&handler_name_length), 4);
    request.append(handler_name);
    request.append(reinterpret_cast<char *>(&req_length), 4);
    request.append(move(request_data));
    send(m_fd, request.data(), request.size(), 0);

    string response(256, '\0');
    recv(m_fd, response.data(), response.size(), 0);

    StatusCode resp_state_code;
    memcpy(&resp_state_code, response.data(), 4);
    if (resp_state_code == NOT_FOUND_SERVICE) {
        cout << "not found service" << endl;
        return "";
    } else if (resp_state_code == NOT_FOUND_HANDLER) {
        cout << "not found handler" << endl;
        return "";
    }
    uint32_t resp_length;
    memcpy(&resp_length, response.data() + 4, 4);
    string response_data = response.substr(8, resp_length);
    return response_data;
}

EchoServiceStub::EchoServiceStub(int fd) : m_fd(fd) {}

EchoResponse EchoServiceStub::echo(EchoRequest req) {
    string request_data;
    req.SerializeToString(&request_data);

    string response_data = call("EchoService", "Echo", move(request_data));
    EchoResponse resp;
    resp.ParseFromString(response_data);
    return resp;
}

string EchoServiceStub::call(const string &service_name, const string &handler_name, string request_data) {
    uint32_t service_name_length = service_name.size();
    uint32_t handler_name_length = handler_name.size();
    uint32_t req_length = request_data.size();

    string request;
    request.reserve(256);
    request.append(reinterpret_cast<char *>(&service_name_length), 4);
    request.append(service_name);
    request.append(reinterpret_cast<char *>(&handler_name_length), 4);
    request.append(handler_name);
    request.append(reinterpret_cast<char *>(&req_length), 4);
    request.append(move(request_data));
    send(m_fd, request.data(), request.size(), 0);

    string response(256, '\0');
    recv(m_fd, response.data(), response.size(), 0);

    StatusCode resp_state_code;
    memcpy(&resp_state_code, response.data(), 4);
    if (resp_state_code == NOT_FOUND_SERVICE) {
        cout << "not found service" << endl;
        return "";
    } else if (resp_state_code == NOT_FOUND_HANDLER) {
        cout << "not found handler" << endl;
        return "";
    }
    uint32_t resp_length;
    memcpy(&resp_length, response.data() + 4, 4);
    string response_data = response.substr(8, resp_length);
    return response_data;
}