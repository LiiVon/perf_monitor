#pragma once

#include "monitor/monitor_inter.h"

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// 前向声明 Skeleton 结构体（由 net_stats.skel.h 定义）
struct net_stats_bpf;

namespace monitor
{

/**
 * 基于 eBPF 的网络流量监控器
 *
 * 使用 eBPF TC hook 挂载到内核网络路径，
 * 实时统计每个网卡的收发流量。
 */
class NetEbpfMonitor : public MonitorInter
{
public:
    NetEbpfMonitor();
    virtual ~NetEbpfMonitor();

    // 核心采集
    void UpdateOnce(monitor::proto::MonitorInfo *monitor_info) override;
    void Stop() override;

    bool IsLoaded();

private:
    // 缓存结构 
    struct NetStatCache
    {
        uint64_t rcv_bytes;      // 接收字节数
        uint64_t rcv_packets;    // 接收包数
        uint64_t snd_bytes;      // 发送字节数
        uint64_t snd_packets;    // 发送包数
        std::chrono::steady_clock::time_point timestamp;  // 采集时间戳
    };

    // 初始化
    bool InitEbpf(); 

    // 清理
    void CleanupEbpf();
    std::string GetIfName(uint32_t ifindex);

    // 统计缓存：key 是网卡索引（ifindex），value 是上一次采集的快照。用于计算速率。
    std::unordered_map<uint32_t, NetStatCache> cache_;

    // 网卡名缓存：因为内核只给 ifindex（数字），用户态需要转成 "ens33" 这样的名字，if_index_to_name 有开销，所以缓存起来。
    std::unordered_map<uint32_t, std::string> ifindex_to_name_;

    // 已挂载的网卡列表：记录哪些网卡成功挂载了 eBPF TC hook，用于清理时逐一 detach。
    std::vector<uint32_t> attached_ifindexes_;

    // 上一次采集的时间戳，用于计算速率。
    std::chrono::steady_clock::time_point last_update_time_;

    // Skeleton 指针：由 libbpf 生成，包含了 eBPF 程序的句柄、Map 的 fd、程序的 fd 等。
    struct net_stats_bpf *skel_;   

    // 加载状态标志：true 表示 eBPF 程序已成功加载并挂载，false 表示失败（会回退到 /proc/net/dev）。
    bool loaded_ = false;

    // BPF Map 的文件描述符
    int map_fd_ = -1;
};

} 