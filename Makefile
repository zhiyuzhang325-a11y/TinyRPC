CXX = g++
CXXFLAGS = -std=c++20 -Iinclude -Ibuild
LIBS = -lprotobuf -lzookeeper_mt -lpthread

PROTO_SRC = build/message.pb.cc

SERVER_SRC = example/server.cpp src/rpc_server.cpp src/sub_reactor.cpp src/service_impl.cpp src/logger.cpp src/thread_pool.cpp src/task.cpp $(PROTO_SRC)
CLIENT_SRC = example/client.cpp src/stub.cpp src/rpc_conn_pool.cpp $(PROTO_SRC)

all: proto server client

proto:
	protoc --cpp_out=build message.proto

server:
	$(CXX) $(CXXFLAGS) $(SERVER_SRC) -o build/server.out $(LIBS)

client:
	$(CXX) $(CXXFLAGS) $(CLIENT_SRC) -o build/client.out $(LIBS)

clean:
	rm -f build/*.out

.PHONY: all proto server client clean