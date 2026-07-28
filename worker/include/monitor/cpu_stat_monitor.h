#pragma once

#include "monitor/monitor_inter.h"
#include "monitor_info.pb.h"
#include "monitor_structs.h"
#include <string>
#include <unordered_map>

namespace monitor
{
    class CpuStatMonitor : public MonitorInter
    {
    public:
        CpuStatMonitor() = default;
        virtual ~CpuStatMonitor() = default;

        void UpdateOnce(monitor::proto::MonitorInfo* monitor_info) override;
        void Stop() override;
    private:
        // cpu0->cpu_stat, cpu1->cpu_stat, ...
        std::unordered_map<std::string, struct cpu_stat> cpu_stats_;
    };
}