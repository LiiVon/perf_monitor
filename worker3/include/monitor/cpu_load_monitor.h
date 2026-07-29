#pragma once 

#include <string>
#include "monitor/monitor_inter.h"
#include "monitor_info.pb.h"

namespace monitor
{
    class CpuLoadMonitor:public MonitorInter
    {
    public:
        CpuLoadMonitor() = default;
        virtual ~CpuLoadMonitor() = default;

        void UpdateOnce(monitor::proto::MonitorInfo* monitor_info) override;
        void Stop() override;

    private:
        float load_avg_1_ = 0.0f; // 1分钟平均负载
        float load_avg_5_ = 0.0f; // 5分钟平均负载
        float load_avg_15_ = 0.0f; // 15分钟平均负载
    };
}