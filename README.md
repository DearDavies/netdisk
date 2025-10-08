## 项目概览
一个用 C 语言编写的“网盘”示例项目，包含客户端与服务端两部分：
- 客户端从 `config.ini` 读取服务端地址，建立 TCP 连接，完成登录/注册与命令交互（如 `ls`/`cd`/`mkdir`/`put`/`get`/`rm`/`pwd`）。
- 服务端采用 `epoll + 线程池 + 任务队列` 的并发模型处理客户端连接，支持优雅关闭与可配置的文件根目录。

本项目适合学习/实践：socket 网络编程、epoll I/O 多路复用、线程池与生产者-消费者模型、INI 配置解析、统一日志体系等。

## 目录结构
```
netdisk/
  client/
    client.c            # 客户端入口
    client.h            # 客户端公共类型/声明
    config.ini          # 客户端配置（服务端地址与日志等级）
    logger.c/.h         # 日志组件（统一等级输出）
    read_config.c/.h    # INI 配置解析
    util.c/.h           # 客户端工具与状态管理
    subroutine.c/.h     # 登录/注册/命令解析与分发
    MACRO.h             # 客户端宏与常量
    modules/            # 命令模块（cd/get/ls/mkdir/put/pwd/rm）
  server/
    server_main.c       # 服务端入口（epoll + 线程池 + 管道信号）
    util.c              # socket/epoll 辅助（init_socket, add_epoll）
    threadpool.c/.h     # 线程池实现
    queue.c/.h          # 任务队列（生产者-消费者）
    work.c/.h           # 子线程处理逻辑（协议解析与执行）
    read_config.c/.h    # INI 配置解析
    logger.c/.h         # 日志组件
    head.h              # 服务端公共包含
    config.ini          # 服务端配置（监听地址/端口/根目录/日志等级）
  CMakeLists.txt        # CMake 构建配置
  README.md
```

## 功能特性
- 客户端：
  - 配置驱动（从 `client/config.ini` 读取 `server.ip_address`、`server.port`）
  - 登录/注册交互（`subroutine` 流程），支持退出
  - 命令读取、解析与分发到 `modules/` 实现
  - 统一日志输出（支持 `ERROR/WARNING/INFO/DEBUG`）
- 服务端：
  - `fork` 创建父/子进程：父进程仅负责 SIGINT 捕获并通过管道通知子进程；子进程后台化接管业务
  - `epoll` 监听监听套接字与管道读端；`accept` 新连接入队
  - 线程池从队列取出连接，执行 `do_work` 处理请求
  - 支持优雅退出：收到 SIGINT 后向队列投递终止令牌并回收线程
  - 可配置根目录 `server.base_path`（例如 `/tmp/netdisk_files`）

## 构建
项目使用 CMake 构建，已适配类 Unix 环境（Linux/macOS）。

```bash
mkdir -p cmake-build-debug
cd cmake-build-debug
cmake ..
cmake --build . -j
```

默认会生成可执行程序（名称以 CMake 配置为准，例如 `client_app` 和 `server_app`）。若你使用 CLion，直接用 IDE 的 CMake Profiles 构建运行即可。

## 运行
1) 准备服务端运行目录与配置：
- 编辑 `server/config.ini`：
  - `server.ip_address = 0.0.0.0`（监听所有网卡）
  - `server.port = 8563`
  - `server.base_path = /tmp/netdisk_files`（服务端文件根目录）
- 确保根目录存在：
```bash
mkdir -p /tmp/netdisk_files
```

2) 启动服务端：
```bash
./server_app   # 或使用 IDE 运行 server 的 target
```
服务端启动后后台化运行，父进程拦截 Ctrl-C（SIGINT）并通过管道通知子进程优雅退出。

3) 配置并启动客户端：
- 编辑 `client/config.ini`：
  - `server.ip_address = <服务端IP>`（本机可写 `127.0.0.1` 或局域网 IP）
  - `server.port = 8563`
```bash
./client_app   # 或使用 IDE 运行 client 的 target
```

## 配置说明（INI）
客户端 `client/config.ini`：
```
[server]
ip_address = 192.168.182.129  # 服务端 IP（示例）
port = 8563

[logging]
log_level = DEBUG              # ERROR | WARNING | INFO | DEBUG（不区分大小写）
```

服务端 `server/config.ini`：
```
[server]
ip_address = 0.0.0.0          # 监听地址
port = 8563                   # 监听端口
base_path = /tmp/netdisk_files# 服务端文件根目录

[logging]
log_level = DEBUG
```

## 客户端交互与命令
客户端启动后：
```
1. 注册；2. 登录；3. 退出
输入要执行的功能编号：
```
登录后可输入命令，由 `subroutine` 解析为内部结构 `order_t` 并分发至 `modules/`：
- `ls [path]`
- `cd <path>`
- `pwd`
- `mkdir <path>`
- `put <local_path> [remote_path]`  # 上传
- `get <remote_path> [local_path]`  # 下载
- `rm <path>`

实际支持命令以各 `modules/*.c` 已实现内容为准。

## 通信协议（简要）
服务端 `work.h` 中定义了基础消息结构：
```c
typedef struct {
    int order_type;
    char paras[4097];
} send_message_t;

typedef enum {
    INVALID, EMPTY, CD, MKDIR, PUT, GET, LS, RM, PWD
} order_type_t;
```
典型流程：
1. 客户端解析用户命令 -> 映射为 `order_type` + 参数 `paras`
2. 通过已建立的 TCP 连接发送消息
3. 服务端线程 `do_work(client_fd, base_path)` 读取消息，根据 `order_type` 路由到相应处理（文件系统操作、数据读写）
4. 将结果（状态码/数据）回传给客户端

注意：消息体边界、文件传输的分片/校验等细节以 `work.c` 的实现为准（建议在此文件中补充/统一协议约定）。

## 并发模型与优雅退出
- 主进程：`server_main.c` 中 `fork()` 后的父进程仅负责 `SIGINT` 捕获，经 `pipe` 将退出指令写给子进程。
- 子进程：后台化（`setpgid(0, 0)`），维护 `epoll_fd` 同时监听监听套接字与 `pipe_fd[0]`。
- 新连接：`accept` 后将客户端 `fd` 入队 `queue`，由线程池工作线程取出处理。
- 退出：收到 `SIGINT` 时，向队列投递若干“终止令牌”（如 `-2`）以唤醒各工作线程，随后回收 `pthread_join` 并退出。

## 关键代码位置（参考）
- 客户端入口：`client/client.c`（配置读取、连接、登录/命令循环）
- 服务端入口：`server/server_main.c`（epoll、线程池、信号与管道）
- 套接字/epoll 工具：`server/util.c`（`init_socket`, `add_epoll`）
- 线程池：`server/threadpool.*`
- 任务队列：`server/queue.*`
- 业务处理：`server/work.*`（含协议枚举与 `do_work` 原型）
- 配置解析：`client/read_config.*`, `server/read_config.*`
- 日志：`client/logger.*`, `server/logger.*`

## 开发与扩展建议
- 明确客户端/服务端协议：为每条命令定义请求/响应结构、错误码与数据格式；文件传输建议增量/分片并校验。
- 完善 `work.c`：补齐各 `order_type` 的处理与异常路径（权限、路径越界、磁盘错误）。
- 安全性：
  - 路径规范化与越权访问防护（限制在 `base_path` 内）。
  - 上传/下载限速与大小限制，防止滥用。
  - 身份认证令牌化（登录后下发 token 并校验）。
- 稳定性：
  - 超时控制与心跳保活。
  - 更细的线程池参数与队列容量控制。
  - 日志滚动与多目标输出（文件/控制台）。

## 故障排查
- 客户端无法连接：
  - 确认 `client/config.ini` 服务端 IP/端口与服务端一致
  - 确认服务端监听地址与防火墙设置
- 服务端 `bind` 失败：
  - 端口被占用；或未设置 `SO_REUSEADDR`
- `epoll_wait` 返回错误：
  - 检查 `add_epoll` 是否成功、文件描述符是否有效
- 线程不退出：
  - 检查是否正确投递与消费“终止令牌”，以及 `pthread_join` 目标是否匹配

## 许可证
学习示例项目，未指定许可证。若需对外发布，请补充 LICENSE 并检查第三方依赖许可。