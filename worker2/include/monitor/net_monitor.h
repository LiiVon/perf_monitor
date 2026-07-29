#pragma once

#include "monitor/monitor_inter.h"
#include "monitor_info.pb.h"
#include <chrono>
#include <string>
#include <unordered_map>

namespace monitor
{
    struct NetStat
    {
        std::string name;
        uint64_t rcv_bytes;
        uint64_t rcv_packets;
        uint64_t snd_bytes;
        uint64_t snd_packets;
        uint64_t err_in;
        uint64_t err_out;
        uint64_t drop_in;
        uint64_t drop_out;
    };
    class NetMonitor : public MonitorInter
    {
    public:
        NetMonitor() = default;
        virtual ~NetMonitor() = default;

        void UpdateOnce(monitor::proto::MonitorInfo *monitor_info) override;
        void Stop() override;

    private:
        struct NetInfo
        {
            std::string name;
            uint64_t rcv_bytes;
            uint64_t rcv_packets;
            uint64_t snd_bytes;
            uint64_t snd_packets;
            uint64_t err_in;
            uint64_t err_out;
            uint64_t drop_in;
            uint64_t drop_out;
            std::chrono::steady_clock::time_point timepoint;
        };

    private:
        std::unordered_map<std::string, NetInfo> last_net_info_;
    };
}