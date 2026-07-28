#include "monitor/cpu_stat_monitor.h"
#include "monitor/monitor_structs.h"
#include "monitor_info.grpc.pb.h"
#include "monitor_info.pb.h"
#include "utils/read_file.h"
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace monitor
{
    static const char *CPU_STAT_DEVICE = "/dev/cpu_stat_monitor";
    static constexpr size_t MAX_CPUS = 256;

    void CpuStatMonitor::UpdateOnce(monitor::proto::MonitorInfo *monitor_info)
    {
        int fd = open(CPU_STAT_DEVICE, O_RDONLY);
        if (fd < 0)
        {
            // 设备不存在，可能内核模块未加载
            return;
        }

        size_t map_size = sizeof(struct cpu_stat) * MAX_CPUS;
        void *addr = mmap(nullptr, map_size, PROT_READ, MAP_SHARED, fd, 0);
        if (addr == MAP_FAILED)
        {
            close(fd);
            return;
        }

        struct cpu_stat *stats = static_cast<struct cpu_stat *>(addr);
        for (size_t i = 0; i < MAX_CPUS; ++i)
        {
            if (stats[i].cpu_name[0] == '\0')
            {
                break;
            }

            auto it = cpu_stats_.find(stats[i].cpu_name);
            if (it != cpu_stats_.end())
            {
                struct cpu_stat &old = it->second;
                auto cpu_stat_msg = monitor_info->add_cpu_stat();

                float new_cpu_total_time =
                    stats[i].user + stats[i].nice + stats[i].system + stats[i].idle +
                    stats[i].iowait + stats[i].irq + stats[i].softirq + stats[i].steal;

                float old_cpu_total_time =
                    old.user + old.nice + old.system + old.idle +
                    old.iowait + old.irq + old.softirq + old.steal;

                float new_cpu_busy_time =
                    stats[i].user + stats[i].nice + stats[i].system +
                    stats[i].irq + stats[i].softirq + stats[i].steal;

                float old_cpu_busy_time =
                    old.user + old.nice + old.system +
                    old.irq + old.softirq + old.steal;

                float cpu_percent = (new_cpu_busy_time - old_cpu_busy_time) / (new_cpu_total_time - old_cpu_total_time) * 100.0f;
                float cpu_user_percent = (stats[i].user - old.user) / (new_cpu_total_time - old_cpu_total_time) * 100.0f;
                float cpu_nice_percent = (stats[i].nice - old.nice) / (new_cpu_total_time - old_cpu_total_time) * 100.0f;
                float cpu_system_percent = (stats[i].system - old.system) / (new_cpu_total_time - old_cpu_total_time) * 100.0f;
                float cpu_idle_percent = (stats[i].idle - old.idle) / (new_cpu_total_time - old_cpu_total_time) * 100.0f;
                float cpu_iowait_percent = (stats[i].iowait - old.iowait) / (new_cpu_total_time - old_cpu_total_time) * 100.0f;
                float cpu_irq_percent = (stats[i].irq - old.irq) / (new_cpu_total_time - old_cpu_total_time) * 100.0f;
                float cpu_softirq_percent = (stats[i].softirq - old.softirq) / (new_cpu_total_time - old_cpu_total_time) * 100.0f;

                cpu_stat_msg->set_cpu_name(stats[i].cpu_name);
                cpu_stat_msg->set_cpu_percent(cpu_percent);
                cpu_stat_msg->set_usr_percent(cpu_user_percent);
                cpu_stat_msg->set_nice_percent(cpu_nice_percent);
                cpu_stat_msg->set_system_percent(cpu_system_percent);
                cpu_stat_msg->set_idle_percent(cpu_idle_percent);
                cpu_stat_msg->set_io_wait_percent(cpu_iowait_percent);
                cpu_stat_msg->set_irq_percent(cpu_irq_percent);
                cpu_stat_msg->set_soft_irq_percent(cpu_softirq_percent);
            }

            // 更新缓存
            cpu_stats_[stats[i].cpu_name] = stats[i];

        }
        munmap(addr, map_size);
        close(fd);
        return;
    }

    void CpuStatMonitor::Stop()
    {
        // 这里可以实现一些清理工作，如果有需要的话
    }
}