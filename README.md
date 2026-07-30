# 基于eBPF与内核模块的 Linux 服务器性能监控系统

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![gRPC](https://img.shields.io/badge/gRPC-1.50+-green.svg)](https://grpc.io/)
[![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)](https://www.linux.org/)
[![Qt](https://img.shields.io/badge/Qt-6.10+-brightgreen.svg)](https://www.qt.io/)

分布式服务器性能监控系统，采用 **Push 模式** 架构，支持多服务器性能数据采集、存储和实时展示。提供 Qt 前端看板，实现数据可视化监控。

---

## ✨ 特性

- 🚀 **高效采集** — 结合 `/proc` 文件系统、内核模块 `mmap` 零拷贝、eBPF TC Hook 三种采集方式
- 📊 **全面监控** — CPU、内存、磁盘、网络、软中断等全方位指标
- 🔄 **Push 模式** — Worker 主动推送，降低 Manager 负载，支持水平扩展
- 📈 **健康评分** — 多维度加权评分算法（CPU 35% + 内存 30% + 负载 15% + 磁盘 15% + 网络 5%）
- 🖥️ **Qt 可视化看板** — 浅色主题，6 个 Tab 页展示完整数据，支持多主机切换
- 🔌 **双接口支持** — gRPC (50051) + HTTP/JSON (50052)，便于前端集成
- 💾 **数据持久化** — MySQL 存储历史数据，支持 LRU 缓存淘汰
- ⚡ **eBPF 网络监控** — 基于 TC Hook 的零拷贝网络流量统计
- 🧠 **自动降级** — mmap/eBPF 失败自动回退到 `/proc`，保证高可用

---

## 📐 系统架构
### 1. 系统整体架构

``` text
┌─────────────────────────────────────────────────────────────────────────────┐
│                           被监控服务器集群                                  │
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
│                           管理端 (Manager)                                 │
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
│  ┌───────────────────┐ ┌───────────────┐ ┌───────────────────────────────┐│
│  │  QueryService     │ │  HTTP Server  │ │     Qt UI                     ││
│  │  (gRPC 50051)     │ │  (JSON 50052) │ │  6 个 Tab: 概览/CPU/内存/     ││
│  │  历史数据查询     │ │  /api/latest  │ │  网络/磁盘/软中断             ││
│  └───────────────────┘ └───────────────┘ └───────────────────────────────┘│
│                                 │                                          │
│                                 ▼                                          │
│                          ┌─────────────┐                                  │
│                          │   MySQL     │                                  │
│                          │ monitor_db  │                                  │
│                          └─────────────┘                                  │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

### 2. 内核模块采集架构（CPU / 软中断）

``` text
┌─────────────────────────────────────────────────────────────────────────────┐
│                           内核模块采集架构                                  │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ cpu_stat_collector.ko / softirq_collector.ko                      │   │
│  │                                                                   │   │
│  │  1. 模块加载时：                                                  │   │
│  │     ├─ 分配共享内存（PAGE_SIZE 对齐）                            │   │
│  │     ├─ 注册字符设备 /dev/cpu_stat_monitor                        │   │
│  │     └─ 启动定时器（delayed_work，每秒一次）                      │   │
│  │                                                                   │   │
│  │  2. 定时器回调：                                                  │   │
│  │     ├─ 遍历所有在线 CPU                                          │   │
│  │     ├─ 从 kcpustat_cpu 读取累计时间（纳秒）                      │   │
│  │     ├─ 转换为 jiffies（内核时钟滴答数）                          │   │
│  │     └─ 填充到共享内存结构体数组                                   │   │
│  │                                                                   │   │
│  │  3. mmap 回调：                                                   │   │
│  │     ├─ remap_pfn_range 建立映射                                  │   │
│  │     ├─ pgprot_noncached 禁用缓存（保证一致性）                   │   │
│  │     └─ 用户态零拷贝读取                                          │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ 关键技术点                                                         │   │
│  │  ├─ delayed_work 替代 hrtimer（兼容性更好）                       │   │
│  │  ├─ 原子操作保证多 CPU 并发安全                                  │   │
│  │  └─ 降级机制：open 失败自动回退到 /proc                          │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

### 3. eBPF 网络采集架构

``` text
┌─────────────────────────────────────────────────────────────────────────────┐
│                           eBPF 网络采集架构                                 │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ net_stats.bpf.c (内核态 eBPF 程序)                                │   │
│  │                                                                   │   │
│  │  ┌─────────────────┐              ┌─────────────────┐             │   │
│  │  │ TC Ingress Hook │              │ TC Egress Hook  │             │   │
│  │  │ (收包时触发)    │              │ (发包时触发)    │             │   │
│  │  └────────┬────────┘              └────────┬────────┘             │   │
│  │           │                                 │                      │   │
│  │           └────────────┬────────────────────┘                      │   │
│  │                        │                                           │   │
│  │                        ▼                                           │   │
│  │              ┌─────────────────────┐                               │   │
│  │              │ update_stats()      │                               │   │
│  │              │ 原子操作累加计数    │                               │   │
│  │              └──────────┬──────────┘                               │   │
│  │                         │                                          │   │
│  │                         ▼                                          │   │
│  │              ┌─────────────────────┐                               │   │
│  │              │   BPF Map           │                               │   │
│  │              │ (net_stats_map)     │                               │   │
│  │              │ key: ifindex        │                               │   │
│  │              │ value: net_stats    │                               │   │
│  │              └─────────────────────┘                               │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                        │
│                                    │ bpf_map_lookup_elem                   │
│                                    ▼                                        │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ NetEbpfMonitor (用户态 Worker)                                    │   │
│  │  ├─ 通过 Skeleton 加载 eBPF 程序到内核                           │   │
│  │  ├─ 定时从 BPF Map 读取统计数据                                  │   │
│  │  ├─ 计算差值得到速率 (diff / time_diff)                          │   │
│  │  └─ 填充 NetInfo Protobuf 消息                                   │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ 关键技术点                                                         │   │
│  │  ├─ TC Hook 比 kprobe 更稳定（网络子系统原生接口）                │   │
│  │  ├─ 原子操作保证多 CPU 并发安全                                  │   │
│  │  ├─ libbpf Skeleton 自动生成加载代码                             │   │
│  │  └─ 降级机制：加载失败自动回退到 /proc/net/dev                   │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

### 4. gRPC + HTTP 通信架构

``` text
┌─────────────────────────────────────────────────────────────────────────────┐
│                              双框架通信流程                                 │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                         Worker (被监控机器)                        │   │
│  │                                                                   │   │
│  │  1. MetricCollector 采集所有监控数据                              │   │
│  │  2. MonitorPusher 定时触发 (每 3 秒)                             │   │
│  │  3. stub->SetMonitorInfo(info)  ← gRPC 客户端调用                │   │
│  └──────────────────────────────┬──────────────────────────────────────┘   │
│                                 │                                          │
│                                 │ gRPC (HTTP/2 + Protobuf)                │
│                                 ▼                                          │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                        Manager (管理端)                            │   │
│  │                                                                   │   │
│  │  ┌─────────────────────────────────────────────────────────────┐   │   │
│  │  │              gRPC 服务 (端口 50051)                       │   │   │
│  │  │                                                             │   │   │
│  │  │  GrpcServerImpl::SetMonitorInfo()                         │   │   │
│  │  │    1. 校验主机名                                           │   │   │
│  │  │    2. 存入 host_data_ 内存缓存                             │   │   │
│  │  │    3. 同步回调 → HostManager::OnDataReceived()            │   │   │
│  │  │    4. 返回 Empty 响应                                      │   │   │
│  │  └─────────────────────────────────────────────────────────────┘   │   │
│  │                              │                                     │   │
│  │                              ▼                                     │   │
│  │  ┌─────────────────────────────────────────────────────────────┐   │   │
│  │  │          HostManager 业务处理                              │   │   │
│  │  │                                                             │   │   │
│  │  │  接收 gRPC 推送的数据 → 处理 → 存储                        │   │   │
│  │  │                                                             │   │   │
│  │  │  ├─ ExtractHostName (主机标识)                             │   │   │
│  │  │  ├─ CalcScore (评分)                                       │   │   │
│  │  │  ├─ ComputeRates (变化率)                                  │   │   │
│  │  │  ├─ 内存缓存 (host_scores_)                                │   │   │
│  │  │  └─ WriteToMysql (持久化)                                  │   │   │
│  │  └──────────────────────────────┬──────────────────────────────┘   │   │
│  │                                 │                                  │   │
│  │                     ┌───────────┴───────────┐                      │   │
│  │                     │                       │                      │   │
│  │                     ▼                       ▼                      │   │
│  │  ┌──────────────────────────┐ ┌──────────────────────────────┐   │   │
│  │  │  MySQL 数据库            │ │  HTTP 服务 (端口 50052)      │   │   │
│  │  │  (持久化存储)            │ │  cpp-httplib 单头文件库      │   │   │
│  │  │                          │ │                              │   │   │
│  │  │  5 张表:                 │ │  GET /api/latest             │   │   │
│  │  │  ├─ server_performance   │ │    1. GetAllHostScores()   │   │   │
│  │  │  ├─ server_net_detail    │ │    2. 转换为 JSON 数组      │   │   │
│  │  │  ├─ server_disk_detail   │ │    3. 返回 HTTP 200        │   │   │
│  │  │  ├─ server_mem_detail    │ │                              │   │   │
│  │  │  └─ server_softirq_detail│ │                              │   │   │
│  │  └──────────────────────────┘ └──────────────┬───────────────┘   │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                 │                                          │
│                                 │ HTTP / JSON                             │
│                                 ▼                                          │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                         Qt UI (前端)                               │   │
│  │                                                                   │   │
│  │  1. QTimer 定时 (每 3 秒)                                        │   │
│  │  2. fetchData() → GET /api/latest                                │   │
│  │  3. 解析 JSON → 更新 4 个图表 + 5 个卡片 + 6 个表格             │   │
│  │  4. 下拉框切换主机 → 重新加载数据                                │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

### 5. Qt UI 交互架构

``` text
┌─────────────────────────────────────────────────────────────────────────────┐
│                            Qt UI 交互架构                                   │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ 主窗口 (MainWindow)                                               │   │
│  │  ├─ 顶部状态栏: 状态标签 + 在线主机数 + 主机下拉框 + 刷新按钮     │   │
│  │  ├─ TabWidget: 6 个 Tab 页                                        │   │
│  │  └─ 定时器: 每 3 秒调用 fetchData()                              │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                        │
│                                    ▼                                        │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ 数据获取流程                                                       │   │
│  │  1. fetchData() → HTTP GET /api/latest                            │   │
│  │  2. onReplyFinished() → 解析 JSON 数组                            │   │
│  │  3. hostSelector 下拉框填充主机列表                               │   │
│  │  4. updateAllForCurrentHost() → 更新当前主机数据                  │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ Tab 页内容                                                         │   │
│  ├─ 概览: 5 个卡片 + 4 个趋势图 (CPU/内存/网络/磁盘)                │   │
│  ├─ CPU: 表格 (9 列)                                                 │   │
│  ├─ 内存: 表格 (4 列)                                                │   │
│  ├─ 网络: 表格 (8 列)                                                │   │
│  ├─ 磁盘: 表格 (8 列)                                                │   │
│  └─ 软中断: 表格 (11 列)                                             │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ 浅色主题风格                                                       │   │
│  │  ├─ 背景色: #f5f7fa                                               │   │
│  │  ├─ 卡片: 白色 + 圆角 8px + 浅灰色边框                           │   │
│  │  ├─ Tab 选中: 蓝色下划线 (#2986d8)                               │   │
│  │  ├─ 刷新按钮: 青绿渐变 (#1abc9c → #16a085)                      │   │
│  │  └─ 图表: 浅色主题 (QChart::ChartThemeLight)                     │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

### 6. 完整数据流

``` text
┌─────────────────────────────────────────────────────────────────────────────┐
│                              完整数据流                                     │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ 内核态采集                                                         │   │
│  │  ├─ 网卡收到数据包 → TC Ingress Hook 触发 → eBPF 累加统计        │   │
│  │  ├─ 内核模块定时器 (1秒) → 更新共享内存数据                       │   │
│  │  └─ mmap / bpf_map_lookup_elem 暴露给用户态                       │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                        │
│                                    ▼                                        │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ Worker 采集循环 (每 3 秒)                                         │   │
│  │  ├─ CpuStatMonitor   → mmap 或 /proc/stat                        │   │
│  │  ├─ CpuSoftIrqMonitor → mmap 或 /proc/softirqs                   │   │
│  │  ├─ NetEbpfMonitor   → eBPF 或 /proc/net/dev                     │   │
│  │  ├─ MemMonitor       → /proc/meminfo                              │   │
│  │  ├─ DiskMonitor      → /proc/diskstats                            │   │
│  │  └─ stub->SetMonitorInfo(info) → gRPC 推送                       │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                        │
│                                    │ gRPC (Protobuf)                       │
│                                    ▼                                        │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ Manager 接收与处理                                                 │   │
│  │  ├─ GrpcServerImpl::SetMonitorInfo() → 入队立即返回               │   │
│  │  ├─ WorkerThread 异步处理                                         │   │
│  │  │   └─ HostManager::OnDataReceived()                             │   │
│  │  │       ├─ ExtractHostName()                                     │   │
│  │  │       ├─ CalcScore()                                           │   │
│  │  │       ├─ ComputeRates()                                        │   │
│  │  │       ├─ 内存缓存                                               │   │
│  │  │       └─ WriteToMysql()                                        │   │
│  │  └─ HTTP Server (50052) → /api/latest 返回 JSON                  │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                        │
│                                    │ HTTP / JSON                           │
│                                    ▼                                        │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ Qt UI (每 3 秒轮询)                                               │   │
│  │  ├─ fetchData() → GET /api/latest                                │   │
│  │  ├─ 解析 JSON → 更新 4 个趋势图 + 5 个卡片 + 6 个表格            │   │
│  │  └─ 主机切换 → 清空历史 → 重新加载                                │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
```
---

## 📊 采集方式对比

| 监控项 | 主采集方式 | 备用方式 | 特点 |
|--------|-----------|----------|------|
| **CPU 状态** | 内核模块 `mmap` | `/proc/stat` | 零拷贝，极低延迟 |
| **CPU 负载** | `/proc/loadavg` | — | 标准接口 |
| **软中断** | 内核模块 `mmap` | `/proc/softirqs` | 零拷贝，极低延迟 |
| **内存** | `/proc/meminfo` | — | 标准接口 |
| **磁盘** | `/proc/diskstats` | — | 标准接口 |
| **网络** | eBPF TC Hook | `/proc/net/dev` | 零拷贝，内核原生 |
| **主机/用户** | 系统调用 | — | 标准接口 |

---

## 📁 项目结构

``` text
perf_monitor/
├── worker/                    # 工作者服务器（被监控机器）
│   ├── include/               # 头文件
│   │   ├── monitor/           # 监控器接口
│   │   ├── rpc/               # RPC 客户端
│   │   └── utils/             # 工具类 (ReadFile)
│   ├── src/
│   │   ├── monitor/           # 各监控器实现
│   │   │   ├── cpu_*_monitor.cpp
│   │   │   ├── mem_monitor.cpp
│   │   │   ├── disk_monitor.cpp
│   │   │   ├── net_monitor.cpp      # /proc/net/dev 实现
│   │   │   └── net_ebpf_monitor.cpp # eBPF 实现
│   │   ├── rpc/               # 数据推送 (monitor_pusher)
│   │   ├── utils/             # ReadFile 工具
│   │   ├── kmod/              # 内核模块源码
│   │   │   ├── cpu_stat_collector.c
│   │   │   ├── softirq_collector.c
│   │   │   └── Makefile
│   │   └── ebpf/              # eBPF 程序
│   │       ├── net_stats.bpf.c
│   │       ├── net_stats.h
│   │       └── Makefile
│   └── CMakeLists.txt
│
├── manager/                   # 管理者服务器
│   ├── include/               # 头文件
│   ├── src/
│   │   ├── host_manager.cpp   # 核心业务
│   │   ├── query_manager.cpp  # 数据库查询
│   │   ├── rpc/
│   │   │   ├── grpc_server.cpp
│   │   │   └── query_service.cpp
│   │   └── main.cpp
│   └── CMakeLists.txt
│
├── proto/                     # Protobuf/gRPC 定义
│   ├── monitor_info.proto
│   ├── query_api.proto
│   └── CMakeLists.txt
│
├── monitor_UI/                # Qt 前端项目
│   ├── mainwindow.h/cpp
│   ├── main.cpp
│   └── CMakeLists.txt
│
├── build/                     # 统一构建目录
└── README.md
```

---

## 🔧 环境要求

| 组件 | 版本要求 | 用途 |
|------|----------|------|
| **操作系统** | Linux (Ubuntu 20.04+ / CentOS 8+) | 运行 Worker/Manager |
| **编译器** | GCC 9+ 或 Clang 10+ (C++17) | 编译所有模块 |
| **CMake** | 3.10+ | 构建系统 |
| **MySQL** | 8.0+ (可选) | 数据持久化 |
| **Qt** | 6.5+ (可选) | 前端 UI |
| **gRPC** | 1.50+ | RPC 通信 |
| **Protobuf** | 3.21+ | 序列化 |
| **内核版本** | 5.4+ | eBPF 支持 |
| **Linux 内核头文件** | 与内核匹配 | 内核模块编译 |
| **clang/LLVM** | 12+ | eBPF 程序编译 |
| **libbpf** | 1.0+ | eBPF 加载 |

---

## 📦 安装

### 1. 基础依赖安装

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y \
    build-essential cmake \
    libprotobuf-dev protobuf-compiler \
    libgrpc++-dev protobuf-compiler-grpc \
    libmysqlclient-dev \
    git \
    linux-headers-$(uname -r) \
    clang llvm \
    libbpf-dev \
    linux-tools-common linux-tools-$(uname -r)
```

> `linux-tools-$(uname -r)` 提供 `bpftool`，用于 eBPF 调试。

### 2. 数据库配置

```bash
sudo systemctl start mysql
sudo systemctl enable mysql
sudo mysql -u root -p
```

```sql
CREATE DATABASE monitor_db;
CREATE USER 'monitor'@'localhost' IDENTIFIED BY 'monitor666';
GRANT ALL PRIVILEGES ON monitor_db.* TO 'monitor'@'localhost';
FLUSH PRIVILEGES;
EXIT;
```

导入表结构：
```bash
mysql -u monitor -pmonitor666 monitor_db < manager/sql/init_tables.sql
```

### 3. 克隆项目

```bash
git clone https://github.com/LiiVon/perf_monitor.git
cd perf_monitor
```

---

## 🔨 编译

### 编译内核模块（可选，高性能 CPU 采集）

```bash
cd worker/src/kmod
make
# 生成 cpu_stat_collector.ko 和 softirq_collector.ko
```

### 编译 eBPF 程序（可选，网络加速）

```bash
cd worker/src/ebpf
make
# 生成 net_stats.bpf.o 和 net_stats.skel.h
```

### 编译整个系统（推荐）

```bash
mkdir build && cd build
# 启用 eBPF 需提前按照上述编译ebpf
cmake .. -DENABLE_EBPF=ON  
# 不启用 eBPF 
cmake .. 
make -j$(nproc)
```

**产物位置**：
- `build/proto/libmonitor_proto.a`
- `build/worker/worker`
- `build/manager/manager`
- `monitor_UI/build/`（Qt 前端）

---

## 🚀 快速开始

### 1. 加载内核模块（可选）

```bash
cd worker/src/kmod
sudo insmod cpu_stat_collector.ko
sudo insmod cpu_softirq_collector.ko
sudo chmod 666 /dev/cpu_stat_monitor /dev/cpu_softirq_monitor
```

> **注意**：内核模块**不需要** CMake 参数控制。Worker 运行时自动检测 `/dev/` 设备是否存在，存在则走 mmap，不存在则降级到 `/proc`。

### 2. 启动 Manager

```bash
cd build/manager
./manager
```

输出示例：
```
Starting Monitor Client (Manager Mode)...
Listening on: 0.0.0.0:50051
QueryManager: MySQL connection initialized
Monitor Client listening on 0.0.0.0:50051
HTTP server listening on 0.0.0.0:50052
```

### 3. 启动 Worker

```bash
cd build/worker
sudo ./worker 127.0.0.1:50051   # eBPF 需要 root 权限
```

### 4. 启动 Qt 前端

在 Qt Creator 中打开 `monitor_UI/CMakeLists.txt`，构建并运行。

### 5. 验证

```bash
curl http://127.0.0.1:50052/api/latest
```

---

## 🧪 测试

### stress 压力测试

```bash
sudo apt install stress -y
stress --cpu 4 --timeout 60        # CPU 压力
stress --vm 2 --vm-bytes 2G --timeout 30  # 内存压力
stress --io 4 --timeout 30         # I/O 压力
```

### eBPF 手动验证

```bash
cd worker/src/ebpf
sudo tc qdisc add dev ens33 clsact
sudo tc filter add dev ens33 ingress bpf obj net_stats.bpf.o sec tc/ingress
sudo tc filter add dev ens33 egress bpf obj net_stats.bpf.o sec tc/egress
sudo bpftool map show | grep net_stats_map
sudo bpftool map dump id <map_id>
# 生成流量 (ping) 后再次 dump 观察变化
```

### 单机多机模拟

```bash
cp -r worker worker1 worker2 
# 编辑 worker1/src/monitor/metric_collector.cpp，找到构造函数中的 hostname_ 赋值：
# // 原本是自动获取主机名
# // hostname_ = hostname;   // 注释掉或删除

# // 改为硬编码
hostname_ = "worker-1";

同时修改一下worker1 worker2 的cmkae
将worker  的地方 都改成 worker1 2

最后顶层cmakelists  可以将注释取消

add_subdirectory(worker)
add_subdirectory(worker1)
add_subdirectory(worker2)
# ... 其他内容不变 ...
```

---

## 📊 监控指标

| 监控项 | 主采集方式 | 降级方式 |
|--------|-----------|----------|
| CPU 状态 | 内核模块 `mmap` | `/proc/stat` |
| CPU 负载 | `/proc/loadavg` | — |
| 软中断 | 内核模块 `mmap` | `/proc/softirqs` |
| 内存 | `/proc/meminfo` | — |
| 磁盘 | `/proc/diskstats` | — |
| 网络 | eBPF TC Hook | `/proc/net/dev` |
| 主机信息 | 系统调用 | — |
| 用户名 | `/etc/passwd` | — |

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

---

## ⚙️ 配置说明

### 编译选项

| 选项 | 默认 | 说明 |
|------|------|------|
| `ENABLE_MYSQL` | ON | 启用 MySQL 持久化 |
| `ENABLE_EBPF` | OFF | 启用 eBPF 网络监控（需 libbpf） |
| `BUILD_MANAGER` | ON | 编译 Manager |

### 关键参数

| 参数 | 默认值 | 修改位置 |
|------|--------|----------|
| 推送间隔 | 3 秒 | `MonitorPusher` 构造参数 |
| 离线阈值 | 60 秒 | `host_manager.cpp` ProcessLoop |
| gRPC 端口 | 50051 | `main.cpp` |
| HTTP 端口 | 50052 | `main.cpp` |
| 最大缓存主机 | 1000 | `grpc_server.h` |

---

## 🛠️ 技术栈

| 层级 | 技术 |
|------|------|
| **语言** | C++17、C |
| **RPC 框架** | gRPC + Protocol Buffers |
| **数据采集** | Linux procfs、mmap、内核模块、eBPF TC Hook |
| **HTTP 服务** | cpp-httplib (Header-only) |
| **JSON 处理** | nlohmann/json (Header-only) |
| **数据库** | MySQL 8.0+ |
| **前端** | Qt 6.10 + QChart |
| **构建系统** | CMake 3.10+ |
| **内核编程** | Linux Kernel Module、eBPF (libbpf) |

---

## 📄 许可证

MIT License — 详见 [LICENSE](LICENSE) 文件。

