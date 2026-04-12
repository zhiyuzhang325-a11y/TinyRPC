# TinyRPC

从零实现的轻量级 C++ RPC 框架，基于自定义二进制协议、多 Reactor 架构和 ZooKeeper 服务发现。

## 架构概览

```
┌───────────────────────────┐              ┌──────────────────────────────────────┐
  │         Client            │              │              Server                  │
  │                           │              │                                      │
  │  CalcService_Stub         │              │  ┌────────────────────────────────┐  │
  │    add() ──┐  async       │              │  │   MainReactor (RpcServer)      │  │
  │    sub() ──┤              │              │  │   listen / accept              │  │
  │            ▼              │              │  │   round-robin 分发 conn_fd     │  │
  │  RpcStub::call()          │              │  └─────┬──────┬──────┬───────────┘  │
  │    atomic round-robin ────┼───TCP/IP───→ │  eventfd+queue │      │              │
  │                           │              │        ▼       ▼      ▼              │
  │  RpcConnPool              │              │  ┌─────────────────────────────┐     │
  │    PoolEntry (per ip:port)│              │  │  SubReactor × N (epoll ET)  │     │
  │    ConnGuard RAII 归还     │              │  │  粘包处理 + 协议解析          │     │
  │    健康检查 (MSG_PEEK)     │              │  └─────────────┬───────────────┘     │
  │    最大连接数 + cond_var   │              │        enqueue │                     │
  │                           │              │                ▼                     │
  └─────────┬─────────────────┘              │  ┌─────────────────────────────┐     │
            │                                │  │  ThreadPool (worker × M)    │     │
            │                                │  │  handler 执行 → ready_queue │     │
            │                                │  └─────────────┬───────────────┘     │
            │                                │        eventfd │ 通知                │
            │                                │                ▼                     │
            │                                │  SubReactor 取结果 → sendResponse    │
            │                                │                                      │
            │                                │  shared_ptr<const HandlerMap>        │
            │                                │  守护进程 · 优雅关闭 · 异步日志        │
            │                                └──────────────────┬───────────────────┘
            │                                                   │
            │         ┌───────────────────────────┐             │
            └────────→│        ZooKeeper          │←────────────┘
       zoo_get_children│  /TinyRPC/               │ zoo_create (EPHEMERAL|SEQUENCE)
                       │    /CalcService/         │
                       │      node000001 → ip:port│
                       │      node000002 → ip:port│
                       └───────────────────────────┘
```

## 核心特性

### 自定义二进制协议

请求格式：

```
┌───────────┬─────────────┬──────────────┬──────────┬─────────────┬────────┬──────────┬──────┐
│ magic (4) │ total_len(4)│svc_name_len(4)│ svc_name │method_len(4)│ method │args_len(4)│ args │
└───────────┴─────────────┴──────────────┴──────────┴─────────────┴────────┴──────────┴──────┘
```

响应格式：

```
┌────────────────┬──────────────┬──────┐
│ status_code (4)│ data_len (4) │ data │
└────────────────┴──────────────┴──────┘
```

- **魔数 (0x54525043 = "TRPC")**：标识协议边界，用于错位数据检测与重新对齐
- **总长度字段**：快速判断消息完整性，无需逐段解析
- **Protobuf 序列化**：业务数据使用 Protocol Buffers 编解码

### 多 Reactor + 线程池

- **MainReactor**：单线程负责 `accept`，通过轮询将新连接分发给 SubReactor
- **SubReactor × N**：每个 SubReactor 独立线程运行 epoll ET 事件循环，管理各自的连接集合
- **连接分发**：MainReactor 通过 `eventfd` + 队列将 `conn_fd` 传递给 SubReactor，无锁竞争
- **异步业务处理**：SubReactor 将 handler 执行任务提交给线程池，工作线程完成后通过 `eventfd` 通知 SubReactor 发送响应

### 粘包 / 半包处理

- **完整性检查函数**：纯函数设计，通过 `magic + total_len` 判断消息是否完整，返回消息字节数（完整）、0（不完整）或 -1（数据错位）
- **循环处理**：`while (checkComplete(...))` 循环，一次 `recv` 可能包含多条完整消息，逐条处理
- **错位恢复**：检测到非法魔数时，扫描 buffer 找到下一个合法魔数位置，丢弃脏数据重新对齐

### 服务发现 (ZooKeeper)

- **临时顺序节点**：每个 Server 进程启动时在 `/TinyRPC/<ServiceName>/` 下创建 `ZOO_EPHEMERAL | ZOO_SEQUENCE` 节点，进程退出后节点自动删除
- **多机注册**：不同 Server 注册不同的 `ip:port`，客户端通过 `zoo_get_children` 获取全部 provider 地址

### 客户端

- **RpcStub 基类**：封装协议编码、服务发现、负载均衡、连接管理，业务 Stub 只需继承并实现具体方法
- **同步调用**：每个 RPC 方法内部通过 `call()` 同步完成 send/recv，直接返回响应对象。早期版本使用 `std::async` 实现伪异步，压测发现每次调用创建/销毁线程的开销导致 QPS 下降约 78%，改为同步调用后性能提升 3.6 倍
- **Round-Robin 负载均衡**：`atomic<int>` 索引 + `fetch_add` 无锁轮询，随机初始偏移避免多 Stub 实例集中访问同一 provider
- **连接池 (RpcConnPool)**：
  - 按 `ip:port` 分组管理 TCP 连接，RAII 的 `ConnGuard` 自动归还
  - 懒加载：首次调用时建立连接
  - 健康检查：取出连接时通过 `recv(MSG_PEEK | MSG_DONTWAIT)` 检测连接存活，死连接自动关闭
  - 最大连接数限制：每个 provider 限制最大连接数，超限时 `condition_variable` 等待归还
  - 故障标记：`ConnGuard::markBroken()` 标记坏连接，析构时关闭并减少计数，不归还池中

### 服务端可靠性

- **守护进程**：`fork` 模式，子进程异常退出（信号终止）时自动重启，正常退出时停止
- **优雅关闭**：`SIGINT/SIGTERM` → 设置停止标志 → `eventfd` 唤醒各 SubReactor → 线程 join → 资源清理
- **异步日志 (AsyncLogger)**：前后台双 buffer 交换，后台线程批量写盘，支持 `fork` 后通过 `pthread_atfork` 重置

## 压测报告

### 测试环境

- VMware 虚拟机，Intel i9-14900HX，32 核 / 30GB 内存
- Ubuntu 22.04，客户端与服务端同机部署（loopback）
- 测试方法：多线程并发调用 RPC，每线程持有独立 Stub 实例，通过连接池复用连接，固定运行 10 秒统计结果

### 性能优化：移除 std::async 伪异步

早期客户端使用 `std::async(launch::async)` 为每次 RPC 调用创建独立线程，通过 `std::future` 返回结果。压测发现 QPS 仅约 2,000，单次延迟高达 4,500μs。

通过编写绕过客户端框架的 raw test（直接 socket connect/send/recv），确认服务端单次处理延迟仅约 300μs，定位到瓶颈在 `std::async` 每次调用创建/销毁线程的系统开销。移除伪异步改为同步调用后：

| 指标 | 优化前（std::async） | 优化后（同步调用） | 变化 |
|------|----------------------|---------------------|------|
| QPS | ~14,000 | ~33,600 | **+140%** |
| p50 延迟 | ~670μs | ~284μs | **-58%** |

### 并发度测试（SubReactor=4，worker=4）

| 客户端线程数 | QPS | p50 (μs) | p99 (μs) | p999 (μs) |
|-------------|--------|----------|----------|-----------|
| 5 | 28,630 | 167 | 312 | 419 |
| 10 | 33,617 | 284 | 571 | 715 |
| 20 | 41,665 | 449 | 988 | 1,266 |
| 50 | 46,274 | 1,017 | 2,230 | 2,809 |

吞吐量随并发数持续增长，50 线程达到 46,000+ QPS。延迟随并发线性增长，50 线程 p99 仍控制在 2.2ms 以内。

### 服务端配置对比

- **SubReactor 数量**（2 / 4 / 8 / 16，固定 worker=4）：QPS 均在 33,200–35,450 范围，SubReactor=8 略优但差异不显著
- **Worker 线程数**（1 / 2 / 4 / 8 / 16 per SubReactor，固定 SubReactor=4）：QPS 均在 33,600–35,500 范围，无显著差异

结论：轻量级 handler（加法运算）下，服务端处理耗时趋近于零，瓶颈在网络 IO 往返延迟而非服务端计算能力。当业务 handler 变重（如数据库查询、序列化大对象）时，SubReactor 和 worker 数量的配置将产生显著影响。

### 多实例部署（3 实例同机）

| 部署方式 | QPS | p50 (μs) | p99 (μs) |
|---------|--------|----------|----------|
| 单实例 | 33,617 | 284 | 571 |
| 3 实例同机 | 35,863 | 266 | 557 |

同机 3 实例 QPS 略高于单实例，ZooKeeper 服务发现与 Round-Robin 负载均衡工作正常，成功率 100%。多实例部署的价值在于跨机器水平扩展场景。

## 项目结构

```
TinyRPC/
├── include/                # 头文件
│   ├── rpc_server.h        # MainReactor
│   ├── sub_reactor.h       # SubReactor
│   ├── thread_pool.h       # 线程池
│   ├── task.h              # 异步任务
│   ├── stub.h              # 客户端 Stub 基类及业务 Stub
│   ├── rpc_conn_pool.h     # 连接池 + ConnGuard
│   ├── service_impl.h      # 业务实现
│   ├── status_code.h       # 状态码 + 魔数 + HandlerMap 类型
│   └── logger.h            # 异步日志
├── src/                    # 实现
│   ├── rpc_server.cpp
│   ├── sub_reactor.cpp
│   ├── thread_pool.cpp
│   ├── task.cpp
│   ├── stub.cpp
│   ├── rpc_conn_pool.cpp
│   ├── service_impl.cpp
│   └── logger.cpp
├── example/                # 示例
│   ├── server.cpp          # 服务端入口（守护进程 + 信号处理）
│   └── client.cpp          # 客户端入口（异步调用示例）
├── message.proto           # Protobuf 消息定义
├── Makefile
└── build/                  # 编译输出
```

## 依赖

- **C++23**
- **Protocol Buffers** (libprotobuf)
- **ZooKeeper C Client** (libzookeeper_mt)
- **pthread**

## 编译与运行

```bash
# 编译
make all

# 启动 ZooKeeper（需提前安装并运行）
# 默认连接 127.0.0.1:2181

# 启动 Server（默认 127.0.0.1:9090）
./build/server.out

# 指定 IP 和端口（多机 / 多实例部署）
./build/server.out 127.0.0.1 9091
./build/server.out 127.0.0.1 9092

# 运行 Client
./build/client.out
```

## 示例输出

```
# Server 端
server start

# Client 端
1 + 2 = 3
2 - 1 = 1
echo success
```