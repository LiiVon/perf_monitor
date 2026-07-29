#pragma once

#include "monitor/monitor_inter.h"
#include <string>

namespace monitor
{
    class HostInfoMonitor : public MonitorInter
    {
    public:
        HostInfoMonitor() = default;
        virtual ~HostInfoMonitor() = default;

        void UpdateOnce(monitor::proto::MonitorInfo *monitor_info) override;
        void Stop() override;

    private:
        std::string GetHostname();
        std::string GetPrimaryIpAddress();

        std::string cached_hostname_; // 缓存的主机名
        std::string cached_ip_;       // 缓存的 IP 地址
        bool info_cached_ = false;    // 是否已缓存（主机信息通常不变）
    };
}