#include "monitor/metric_collector.h"
#include "monitor/cpu_load_monitor.h"
#include "monitor/cpu_softirq_monitor.h"
#include "monitor/cpu_stat_monitor.h"
#include "monitor/disk_monitor.h"
#include "monitor/host_info_monitor.h"
#include "monitor/mem_monitor.h"
#include "monitor/user_monitor.h"
#include <unistd.h>
#include <memory>

#ifdef ENABLE_EBPF
#include "monitor/net_ebpf_monitor.h"
#else
#include "monitor/net_monitor.h"
#endif

namespace monitor
{
    MetricCollector::MetricCollector()
    {
        // 获取主机名
        // char hostname[256];
        // if (gethostname(hostname, sizeof(hostname)) == 0)
        // {
        //     hostname_ = hostname;
        // }
        // else
        // {
        //     hostname_ = "unknown";
        // }

        // 多机测试 强制改名
        hostname_ = "worker-3";
        // 初始化监控器
        monitors_.emplace_back(std::make_unique<CpuLoadMonitor>());
        monitors_.emplace_back(std::make_unique<CpuSoftIrqMonitor>());
        monitors_.emplace_back(std::make_unique<CpuStatMonitor>());
        monitors_.emplace_back(std::make_unique<MemMonitor>());
        monitors_.emplace_back(std::make_unique<DiskMonitor>());
        monitors_.emplace_back(std::make_unique<HostInfoMonitor>());
        monitors_.emplace_back(std::make_unique<UserMonitor>());
#ifdef ENABLE_EBPF
        monitors_.emplace_back(std::make_unique<NetEbpfMonitor>());
#else
        monitors_.emplace_back(std::make_unique<NetMonitor>());
#endif
    }

    MetricCollector::~MetricCollector()
    {
        for (auto &monitor : monitors_)
        {
            monitor->Stop();
        }
    }

    void MetricCollector::CollectAll(monitor::proto::MonitorInfo *monitor_info)
    {
        if (!monitor_info)
            return;

        // 1. 先让所有监控器填充数据
        for (auto &monitor : monitors_)
        {
            monitor->UpdateOnce(monitor_info);
        }

        // 2. 最后强制设置主机名（覆盖 UserMonitor 可能的修改）
        monitor_info->set_name(hostname_);
    }
}
