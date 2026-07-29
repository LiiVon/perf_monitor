#pragma once

#include "monitor/monitor_inter.h"

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>

struct bpf_object;

namespace monitor
{
    // 基于 eBPF 的网络流量监控器
    // 使用 eBPF tracepoint 挂载到内核网络路径，
    class NetEbpfMonitor : public IMonitor
    {
    public:
        NetEbpfMonitor();
        virtual ~NetEbpfMonitor();

        void UpdateOnce(monitor::proto::MonitorInfo *monitor_info) override;
        void Stop() override;

        bool IsLoaded();

    private:
        // 缓存
        struct NetStatCache
        {
            uint64_t send_bytes;
            uint64_t rcv_bytes;
            uint64_t send_packets;
            uint64_t rcv_packets;
        };

        bool InitEbpf();
        void CleanupEbpf();
        std::string GetIfName(uint32_t ifindex);

        std::unordered_map<uint32_t, NetStatCache> cache_;
        std::unordered_map<uint32_t, std::string> ifindex_to_name_;
        std::vector<uint32> attached_ifindexes_;
        std::chrono::steady_clock::time_point last_update_time_;

        struct bpf_object *bpf_obj_;
        bool loaded_ = false;
        int map_fd = -1;
    };
}
