#pragma once

#include "monitor/monitor_inter.h"
#include "monitor_info.pb.h"
#include "monitor_structs.h"
#include <string>
#include <chrono>
#include <unordered_map>

namespace monitor
{
    // 无法修改 struct softirq_stat 来加入时间戳字段
    // 只能加一个新的 结构体 保存历史采样快照
    struct SoftIrq
    {
        std::string cpu_name;
        int64_t hi;
        int64_t timer;
        int64_t net_tx;
        int64_t net_rx;
        int64_t block;
        int64_t irq_poll;
        int64_t tasklet;
        int64_t sched;
        int64_t hrtimer;
        int64_t rcu;
        // 用于计算速率/差值
        std::chrono::steady_clock::time_point timepoint;
    };
    class CpuSoftIrqMonitor : public MonitorInter
    {
    public:
        CpuSoftIrqMonitor() = default;
        virtual ~CpuSoftIrqMonitor() = default;

        void UpdateOnce(monitor::proto::MonitorInfo *monitor_info) override;
        void Stop() override;

    private:
        // cpu0->softirq_stat, cpu1->softirq_stat, ...
        std::unordered_map<std::string, struct SoftIrq> cpu_softirqs_;
    };
}