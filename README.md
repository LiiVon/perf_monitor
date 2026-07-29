
# Linux 服务器性能监控系统

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![gRPC](https://img.shields.io/badge/gRPC-1.50+-green.svg)](https://grpc.io/)
[![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)](https://www.linux.org/)
[![Qt](https://img.shields.io/badge/Qt-6.10+-brightgreen.svg)](https://www.qt.io/)

分布式服务器性能监控系统，采用 **Push 模式** 架构，支持多服务器性能数据采集、存储和实时展示。提供 Qt 前端看板，实现数据可视化监控。

## ✨ 特性

- 🚀 **高效采集** - 基于 `/proc` 文件系统 + `mmap` 零拷贝（内核模块规划中）
- 📊 **全面监控** - CPU、内存、磁盘、网络、软中断等全方位指标
- 🔄 **Push 模式** - Worker 主动推送，降低 Manager 负载，支持水平扩展
- 📈 **健康评分** - 多维度加权评分算法（CPU 35% + 内存 30% + 负载 15% + 磁盘 15% + 网络 5%）
- 🖥️ **Qt 可视化看板** - 实时监控界面，支持多主机切换
- 🔌 **双接口支持** - gRPC (50051) + HTTP/JSON (50052)，便于前端集成
- 💾 **数据持久化** - MySQL 存储历史数据

## 📐 系统架构

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              被监控服务器集群                               │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐      │
│  │   Worker    │  │   Worker    │  │   Worker    │  │   Worker    │      │
│  │  (server-1) │  │  (server-2) │  │  (server-3) │  │  (server-N) │      │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘      │
│         │                │                │                │              │
│         └────────────────┼────────────────┼────────────────┘              │
│                          │                │                                │
│                     gRPC Push (MonitorInfo)                               │
│                          │                │                                │
└──────────────────────────┼────────────────┼────────────────────────────────┘
                           │                │
                           ▼                ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                       管理端 (Manager + Qt UI)                             │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │                      GrpcServerImpl                                 │  │
│  │             接收 SetMonitorInfo 请求                                │  │
│  └──────────────────────────────┬───────────────────────────────────────┘  │
│                                 │                                          │
│  ┌──────────────────────────────▼───────────────────────────────────────┐  │
│  │                         HostManager                                  │  │
│  │  ├─ 主机标识提取 (ExtractHostName)                                  │  │
│  │  ├─ 健康评分计算 (CalcScore)                                        │  │
│  │  ├─ 内存缓存 (host_scores_)                                         │  │
│  │  ├─ 变化率计算 (ComputeRates)                                       │  │
│  │  └─ MySQL 持久化 (WriteToMysql)                                     │  │
│  └──────────────────────────────┬───────────────────────────────────────┘  │
│                                 │                                          │
│              ┌──────────────────┼──────────────────┐                       │
│              │                  │                  │                       │
│              ▼                  ▼                  ▼                       │
│  ┌───────────────────┐ ┌───────────────┐ ┌───────────────┐                │
│  │  QueryService     │ │  HTTP Server  │ │     Qt UI     │                │
│  │  (gRPC 50051)     │ │  (JSON 50052) │ │  实时看板     │                │
│  │  历史数据查询     │ │  /api/latest  │ │  暗黑风格     │                │
│  └───────────────────┘ └───────────────┘ └───────────────┘                │
│                                 │                                          │
│                                 ▼                                          │
│                          ┌─────────────┐                                  │
│                          │   MySQL     │                                  │
│                          │ monitor_db  │                                  │
│                          └─────────────┘                                  │
└─────────────────────────────────────────────────────────────────────────────┘
```

## 📁 项目结构

```
monitor_system/
├── worker/                    # 工作者服务器（部署在被监控机器）
│   ├── include/
│   │   ├── monitor/           # 监控器接口
│   │   ├── rpc/               # RPC 客户端
│   │   └── utils/             # 工具类 (ReadFile)
│   ├── src/
│   │   ├── monitor/           # 各监控器实现
│   │   │   ├── cpu_load_monitor.cpp
│   │   │   ├── cpu_stat_monitor.cpp
│   │   │   ├── cpu_softirq_monitor.cpp
│   │   │   ├── mem_monitor.cpp
│   │   │   ├── disk_monitor.cpp
│   │   │   ├── net_monitor.cpp
│   │   │   ├── host_info_monitor.cpp
│   │   │   └── user_monitor.cpp
│   │   ├── rpc/               # 数据推送 (monitor_pusher)
│   │   └── utils/             # 工具类实现
│   └── CMakeLists.txt
│
├── manager/                   # 管理者服务器（部署在管理端）
│   ├── include/
│   │   ├── rpc/               # gRPC 服务实现
│   │   └── monitor/           # 业务逻辑
│   ├── src/
│   │   ├── host_manager.cpp   # 核心业务处理
│   │   ├── query_manager.cpp  # 数据库查询
│   │   ├── rpc/
│   │   │   ├── grpc_server.cpp        # gRPC 接收服务
│   │   │   └── query_service.cpp      # 查询服务实现
│   │   └── main.cpp           # 启动 gRPC + HTTP 服务
│   └── CMakeLists.txt
│
├── proto/                     # Protobuf/gRPC 定义
│   ├── monitor_info.proto     # 监控数据定义
│   ├── query_api.proto        # 查询接口定义
│   └── CMakeLists.txt
│
├── monitor_UI/                # Qt 前端项目 (Windows)
│   ├── mainwindow.h/cpp       # 主界面
│   ├── main.cpp
│   └── CMakeLists.txt
│
└── build/                     # 统一构建目录 (可选)
```

## 🔧 环境要求

| 组件 | 版本要求 |
|------|----------|
| **操作系统** | Linux (Ubuntu 20.04+ / CentOS 8+) |
| **编译器** | GCC 9+ 或 Clang 10+ (支持 C++17) |
| **CMake** | 3.10+ |
| **MySQL** | 8.0+ (可选，可关闭) |
| **Qt** | 6.5+ (仅前端需要) |
| **gRPC** | 1.50+ |
| **Protobuf** | 3.21+ |

## 📦 安装

### 1. 依赖安装

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y \
    build-essential cmake \
    libprotobuf-dev protobuf-compiler \
    libgrpc++-dev protobuf-compiler-grpc \
    libmysqlclient-dev \
    git
```

```bash
# CentOS/RHEL
sudo yum install -y \
    gcc-c++ cmake \
    protobuf-devel protobuf-compiler \
    grpc-devel grpc-plugins \
    mariadb-devel \
    git
```

### 2. 数据库配置

#### 2.1 安装并启动 MySQL

```bash
sudo systemctl start mysql
sudo systemctl enable mysql
```

#### 2.2 创建数据库和用户

```bash
sudo mysql -u root -p
```

```sql
-- 创建数据库
CREATE DATABASE monitor_db;

-- 创建用户并授权 (密码: monitor666)
CREATE USER 'monitor'@'localhost' IDENTIFIED BY 'monitor666';
GRANT ALL PRIVILEGES ON monitor_db.* TO 'monitor'@'localhost';
FLUSH PRIVILEGES;
EXIT;
```

#### 2.3 导入表结构

```bash
# 创建 5 张表
mysql -u monitor -pmonitor666 monitor_db < manager/sql/init_tables.sql
```

**表结构说明**：

| 表名 | 说明 |
|------|------|
| `server_performance` | 主性能汇总表 (CPU、内存、负载、评分、变化率) |
| `server_net_detail` | 网络接口详细数据 |
| `server_disk_detail` | 磁盘设备详细数据 |
| `server_mem_detail` | 内存分布详细数据 |
| `server_softirq_detail` | 软中断详细数据 |

#### 2.4 修改数据库连接信息

在 `manager/src/main.cpp` 和 `manager/src/host_manager.cpp` 中确认数据库配置：

```cpp
constexpr char kDefaultMysqlPass[] = "monitor666";  // 与创建用户时的密码一致
```

### 3. 克隆项目

```bash
git clone https://github.com/LiiVon/perf_monitor.git
cd perf_monitor
```

## 🔨 编译

### 方式一：统一构建 (推荐)

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)

# 产物位置
# build/proto/libmonitor_proto.a
# build/worker/worker
# build/manager/manager
```

### 方式二：各模块独立编译

```bash
# 1. Proto
cd proto && mkdir build && cd build
cmake .. && make -j$(nproc)

# 2. Worker
cd ../../worker && mkdir build && cd build
cmake .. && make -j$(nproc)

# 3. Manager
cd ../../manager && mkdir build && cd build
cmake .. && make -j$(nproc)
```

### 方式三：Qt 前端编译 (Windows)

在 Qt Creator 中打开 `monitor_UI/CMakeLists.txt`，配置 CMake 后编译运行。

## 🚀 快速开始

### 1. 启动 Manager

```bash
cd build/manager
./manager

# 或指定监听地址
./manager 0.0.0.0:50051
```

**输出示例**：
```
Starting Monitor Client (Manager Mode)...
Listening on: 0.0.0.0:50051
QueryManager: MySQL connection initialized
Monitor Client listening on 0.0.0.0:50051
HTTP server listening on 0.0.0.0:50052
Waiting for workers to push data...
```

> Manager 同时监听：
> - **50051**: gRPC 服务 (接收 Worker 推送 + 查询接口)
> - **50052**: HTTP 服务 (`/api/latest` 返回所有主机最新数据)

### 2. 启动 Worker

```bash
# 在另一终端
cd build/worker
./worker <Manager_IP>:50051

# 本地测试
./worker 127.0.0.1:50051
```

**输出示例**：
```
Worker connecting to Manager at: 127.0.0.1:50051
MonitorPusher started, pushing to 127.0.0.1:50051 every 3 seconds
================== Collected Metrics ==================
[Host] Hostname: lz-VMware-Virtual-Platform, IP: 192.168.31.135
--- CPU Statistics ---
...
```

### 3. 启动 Qt 前端 (Windows)

在 Qt Creator 中运行 `monitor_UI`，连接 Manager 的 HTTP 端口 (50052)。

### 4. 验证运行

#### 4.1 查看 Manager 终端

```
Received monitor data from: lz-VMware-Virtual-Platform_192.168.31.135
Server: lz-VMware-Virtual-Platform_192.168.31.135, Score: 81.78
```

#### 4.2 HTTP API 验证

```bash
curl http://127.0.0.1:50052/api/latest
```

返回 JSON 数组，包含所有主机的监控数据。

#### 4.3 数据库验证

```bash
mysql -u monitor -pmonitor666 -e "USE monitor_db; SELECT COUNT(*) FROM server_performance;"
```

### 5. 停止服务

```bash
# Worker / Manager 按 Ctrl+C 停止
```

## 🧪 单机多机模拟测试 (开发/测试专用)

在单台机器上模拟多台服务器的监控场景，无需真实多台物理机。

### 原理

通过复制 Worker 目录并硬编码不同的 `hostname_`，让 Manager 认为数据来自不同主机。

### 操作步骤

```bash
# 1. 复制 Worker 目录
cd ~/lz_ws/perf_monitor
cp -r worker worker1
cp -r worker worker2
cp -r worker worker3

# 2. 修改各 Worker 的主机名
# worker1/src/monitor/metric_collector.cpp
hostname_ = "worker-1";

# worker2/src/monitor/metric_collector.cpp
hostname_ = "worker-2";

# worker3/src/monitor/metric_collector.cpp
hostname_ = "worker-3";

# 3. 分别编译
cd worker1/build && cmake .. && make -j$(nproc)
cd worker2/build && cmake .. && make -j$(nproc)
cd worker3/build && cmake .. && make -j$(nproc)

# 4. 分别启动 (三个终端)
./worker1/build/worker1 127.0.0.1:50051
./worker2/build/worker2 127.0.0.1:50051
./worker3/build/worker3 127.0.0.1:50051

# 5. 验证
curl http://127.0.0.1:50052/api/latest
# 应返回 3 个主机的数据
```

> **注意**：`worker1/2/3` 仅用于单机模拟测试。**生产部署时请使用原始的 `worker` 目录**，它会通过 `gethostname()` 自动获取真实主机名，无需任何修改。

## 📊 监控指标

### Worker 采集项

| 监控项 | 数据来源 | 采集内容 |
|--------|----------|----------|
| **CPU 状态** | `/proc/stat` | 各核心使用率、用户态/内核态/空闲/IO等待/硬中断/软中断占比 |
| **CPU 负载** | `/proc/loadavg` | 1/5/15 分钟平均负载 |
| **软中断** | `/proc/softirqs` | 各 CPU 核心软中断统计 (HI/TIMER/NET_TX/NET_RX/BLOCK 等) |
| **内存** | `/proc/meminfo` | 总量、可用、缓存、活跃/非活跃、脏页等 20+ 项指标 |
| **磁盘** | `/proc/diskstats` | 读写速率、IOPS、延迟、利用率、累计读写量 |
| **网络** | `/proc/net/dev` | 收发速率、包数、错误/丢包统计 |
| **主机信息** | 系统调用 | 主机名、主 IP 地址 |
| **用户名** | `/etc/passwd` | 当前进程所属用户名 |

### Manager 查询接口 (gRPC)

| 接口 | 功能 | 用途 |
|------|------|------|
| `QueryPerformance` | 时间段性能数据 | 历史数据分析 |
| `QueryTrend` | 变化率趋势 | 性能趋势预测 |
| `QueryAnomaly` | 异常数据检测 | 告警和问题定位 |
| `QueryScoreRank` | 评分排序 | 服务器选择/调度 |
| `QueryLatestScore` | 最新评分 | 实时状态概览 |
| `QueryNetDetail` | 网络详细数据 | 网络问题排查 |
| `QueryDiskDetail` | 磁盘详细数据 | IO 性能分析 |
| `QueryMemDetail` | 内存详细数据 | 内存使用分析 |
| `QuerySoftIrqDetail` | 软中断详细数据 | 中断负载分析 |

### HTTP API

| 接口 | 方法 | 返回 |
|------|------|------|
| `/api/latest` | GET | 所有主机最新监控数据 (JSON 数组) |

### 健康评分算法

```
Score = CPU_Score × 35% + Mem_Score × 30% + Load_Score × 15% 
      + Disk_Score × 15% + Net_Score × 5%

其中：
- CPU_Score   = 1 - cpu_percent / 100
- Mem_Score   = 1 - mem_used_percent / 100
- Load_Score  = 1 - load_avg_1 / (cpu_cores × 1.5)
- Disk_Score  = 1 - disk_util_percent / 100
- Net_Score   = 1 - bandwidth_usage / max_bandwidth (1Gbps)
```

> 评分范围 0~100，**分值越高表示服务器越健康/空闲**，适合作为负载均衡的决策依据。

## ⚙️ 配置说明

### 服务器标识

Worker 使用 `hostname_` 字段作为服务器唯一标识：

| 部署方式 | 标识来源 | 说明 |
|----------|----------|------|
| **生产部署** | `gethostname()` 自动获取 | 零配置，每台机器主机名不同 |
| **单机模拟** | 硬编码 `worker-1/2/3` | 仅用于测试 |

> `HostManager::ExtractHostName` **优先使用 `info.name()`**，若为空则回退到 `host_info`。

### 关键参数

| 参数 | 默认值 | 说明 | 修改位置 |
|------|--------|------|----------|
| 推送间隔 | 3 秒 | Worker 采集推送周期 | `monitor_pusher.cpp` 构造函数 |
| 离线阈值 | 60 秒 | 超时未上报视为离线 | `host_manager.cpp` ProcessLoop |
| gRPC 端口 | 50051 | Manager 监听端口 | `main.cpp` |
| HTTP 端口 | 50052 | Manager HTTP API 端口 | `main.cpp` |
| 最大缓存主机数 | 1000 | LRU 淘汰阈值 | `grpc_server.h` |

### 编译选项

| 选项 | 默认 | 说明 |
|------|------|------|
| `ENABLE_MYSQL` | ON | 启用 MySQL 持久化 |
| `ENABLE_EBPF` | OFF | 启用 eBPF 网络监控 (需 libbpf) |
| `BUILD_MANAGER` | ON | 编译 Manager |

## 🛠️ 技术栈

| 层级 | 技术 |
|------|------|
| **语言** | C++17 |
| **RPC 框架** | gRPC + Protocol Buffers |
| **数据采集** | Linux procfs + mmap (内核模块规划中) |
| **HTTP 服务** | cpp-httplib (Header-only) |
| **JSON 处理** | nlohmann/json (Header-only) |
| **数据库** | MySQL 8.0+ |
| **前端** | Qt 6.10 + QChart |
| **构建系统** | CMake 3.10+ |

## 📄 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件。

---
