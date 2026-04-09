# TinyRPC

C++ 轻量级 RPC 框架，从零实现远程过程调用，支持服务注册发现、异步调用、连接池复用。

## 架构
```
  ┌───────────────────────┐            ┌───────────────────────┐
  │       Client          │            │       Server          │
  │                       │            │                       │
  │  CalcService_Stub     │            │  ┌─────────────────┐  │
  │    add() ──┐          │            │  │   RpcServer      │  │
  │    sub() ──┤          │            │  │   epoll ET       │  │
  │            ▼          │            │  │   accept + recv  │  │
  │  RpcStub (base)       │            │  │   process        │  │
  │    call() ────────────┼──TCP/IP──→ │  │   sendResponse   │  │
  │    future + async     │            │  └────────┬──────┬──┘  │
  │            │          │            │           │      │     │
  │  RpcConnPool          │            │  handler map     │     │
  │    getConn()          │            │  (2-level umap)  │     │
  │    ConnGuard RAII     │            │           │      │     │
  └───────┬───────────────┘            │  CalcServiceImpl │     │
          │                            │  EchoServiceImpl │     │
          │                            └──────────────────┼─────┘
          │         ┌──────────────┐                      │
          └────────→│  ZooKeeper   │←─────────────────────┘
           zoo_get  │  /TinyRPC/   │  zoo_create
                    │  服务注册发现 │
                    └──────────────┘
```

## 设计说明

自定义协议：请求格式为 `service_name_len | service_name | method_name_len | method_name | args_len | args`，响应格式为 `status_code | data_len | data`，使用字符串标识服务和方法，避免枚举编号冲突，扩展性好。

服务分发：两层嵌套 `unordered_map<string, unordered_map<string, function<string(const string&)>>>`，外层按 service_name 查找，内层按 method_name 查找，O(1) 分发。

Stub 封装：`RpcStub` 基类处理 ZooKeeper 查询、连接管理、协议收发，子类（如 `CalcService_Stub`）只需实现业务方法。调用方一行代码即可发起远程调用。

异步调用：每个业务方法通过 `std::async + std::future` 实现异步，调用方可选择立即 `.get()` 同步等待，或先做其他事后续取结果。

ZooKeeper 服务发现：服务端启动时将 service_name 和地址注册到 ZooKeeper（路径 `/TinyRPC/ServiceName`），客户端 Stub 构造时从 ZooKeeper 查询地址，无需硬编码。

连接池：`RpcConnPool` 按地址管理连接，懒加载创建，`ConnGuard` RAII 自动归还。不同 Stub 共享同一连接池，同地址服务复用连接。锁只保护队列操作，connect 在锁外执行避免阻塞。

epoll ET 服务端：非阻塞 socket + epoll 边缘触发，支持多客户端并发连接。

错误处理：`StatusCode` 枚举区分 OK、NOT_FOUND_SERVICE、NOT_FOUND_HANDLERINVAILD_REQUEST、INTERNAL_ERROR，客户端根据状态码处理异常。recv 设置 `SO_RCVTIMEO` 超时保护。

Protobuf 序列化：请求和响应的 args 部分使用 protobuf 序列化，高效紧凑。

## 目录结构
```
TinyRPC/
├── src/                  # 框架源文件
│   ├── rpc_server.cpp    # RPC 服务端（epoll + 分发）
│   ├── stub.cpp          # RPC 客户端 Stub 基类 + 子类
│   ├── rpc_conn_pool.cpp # 连接池 + ConnGuard
│   ├── service_impl.cpp  # 服务实现（CalcService、EchoService）
│   └── logger.cpp        # 异步日志
├── include/              # 头文件
├── example/              # 示例（server/client 启动入口）
├── build/                # 编译输出 + protobuf 生成文件
├── logs/                 # 运行日志
├── message.proto         # protobuf 定义
├── Makefile
└── README.md
```

## 编译与运行
```bash
# 启动 ZooKeeper
sudo systemctl start zookeeper

# 编译
make

# 启动服务端
./build/server.out

# 启动客户端（另一个终端）
./build/client.out

# 清理
make clean
```

## 测试
```bash
# 客户端输出示例
1 + 2 = 3
2 - 1 = 1
echo success
```

## 环境
- Ubuntu 22.04
- g++ 11.4.0
- protobuf 3.12.4
- ZooKeeper 3.4.13

## 依赖安装
```bash
sudo apt install g++ protobuf-compiler libprotobuf-dev zookeeperd libzookeeper-mt-dev
```