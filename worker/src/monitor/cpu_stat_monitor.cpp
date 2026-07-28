#include "monitor/cpu_stat_monitor.h"
#include "monitor/monitor_structs.h"
#include "monitor_info.grpc.pb.h"
#include "monitor_info.pb.h"
#include "utils/read_file.h"
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <chrono>
#include <cctype> // for isdigit

namespace monitor
{
    static const char *CPU_STAT_DEVICE = "/dev/cpu_stat_monitor";
    static constexpr size_t MAX_CPUS = 256;

    // 辅助函数：填充 CPU 统计消息（与 mmap 和 proc 共用）
    static void FillCpuStatMessage(monitor::proto::CpuStat *msg,
                                   const struct cpu_stat &curr,
                                   const struct cpu_stat *old)
    {
        if (!old)
        {
            // 首次采集，没有历史数据，无法计算百分比，跳过填充（但 msg 已创建）
            // 也可以设置默认值，但留空更合理
            return;
        }
        // 计算总时间差（浮点数防止溢出）
        float new_total = curr.user + curr.nice + curr.system + curr.idle +
                          curr.iowait + curr.irq + curr.softirq + curr.steal;
        float old_total = old->user + old->nice + old->system + old->idle +
                          old->iowait + old->irq + old->softirq + old->steal;
        float delta_total = new_total - old_total;
        if (delta_total <= 0)
            return; // 防止除零

        float new_busy = curr.user + curr.nice + curr.system +
                         curr.irq + curr.softirq + curr.steal;
        float old_busy = old->user + old->nice + old->system +
                         old->irq + old->softirq + old->steal;

        msg->set_cpu_name(curr.cpu_name);
        msg->set_cpu_percent((new_busy - old_busy) / delta_total * 100.0f);
        msg->set_usr_percent((curr.user - old->user) / delta_total * 100.0f);
        msg->set_nice_percent((curr.nice - old->nice) / delta_total * 100.0f);
        msg->set_system_percent((curr.system - old->system) / delta_total * 100.0f);
        msg->set_idle_percent((curr.idle - old->idle) / delta_total * 100.0f);
        msg->set_io_wait_percent((curr.iowait - old->iowait) / delta_total * 100.0f);
        msg->set_irq_percent((curr.irq - old->irq) / delta_total * 100.0f);
        msg->set_soft_irq_percent((curr.softirq - old->softirq) / delta_total * 100.0f);
    }

    void CpuStatMonitor::UpdateOnce(monitor::proto::MonitorInfo *monitor_info)
    {
        // ==================== 优先尝试内核模块 ====================
        int fd = open(CPU_STAT_DEVICE, O_RDONLY);
        if (fd >= 0)
        {
            size_t map_size = sizeof(struct cpu_stat) * MAX_CPUS;
            void *addr = mmap(nullptr, map_size, PROT_READ, MAP_SHARED, fd, 0);
            if (addr != MAP_FAILED)
            {
                struct cpu_stat *stats = static_cast<struct cpu_stat *>(addr);
                for (size_t i = 0; i < MAX_CPUS; ++i)
                {
                    if (stats[i].cpu_name[0] == '\0')
                        break;
                    std::string cpu_name(stats[i].cpu_name);
                    auto it = cpu_stats_.find(cpu_name);
                    auto *msg = monitor_info->add_cpu_stat();
                    if (it != cpu_stats_.end())
                    {
                        FillCpuStatMessage(msg, stats[i], &it->second);
                    }
                    // 更新缓存（无论是否首次）
                    cpu_stats_[cpu_name] = stats[i];
                }
                munmap(addr, map_size);
                close(fd);
                return;
            }
            close(fd);
        }

        

        // ==================== 回退：从 /proc/stat 读取 ====================
        ReadFile rf("/proc/stat");
        std::vector<std::string> args;
        while (rf.ReadLine(&args))
        {
            if (args.size() < 11)
            {
                args.clear();
                continue;
            }
            std::string cpu_name = args[0];
            // 只处理 "cpu" 后跟数字的行（如 cpu0, cpu1...），跳过 "cpu" 总行
            if (cpu_name.size() <= 3 || cpu_name.substr(0, 3) != "cpu")
            {
                args.clear();
                continue;
            }
            if (!isdigit(cpu_name[3]))
            { // 确保是 cpu0, cpu1 等
                args.clear();
                continue;
            }

            // 解析各字段
            struct cpu_stat curr;
            strncpy(curr.cpu_name, cpu_name.c_str(), sizeof(curr.cpu_name) - 1);
            curr.cpu_name[sizeof(curr.cpu_name) - 1] = '\0';
            curr.user = std::stoull(args[1]);
            curr.nice = std::stoull(args[2]);
            curr.system = std::stoull(args[3]);
            curr.idle = std::stoull(args[4]);
            curr.iowait = std::stoull(args[5]);
            curr.irq = std::stoull(args[6]);
            curr.softirq = std::stoull(args[7]);
            curr.steal = std::stoull(args[8]);
            curr.guest = std::stoull(args[9]);
            curr.guest_nice = std::stoull(args[10]);

            auto it = cpu_stats_.find(cpu_name);
            auto *msg = monitor_info->add_cpu_stat();
            if (it != cpu_stats_.end())
            {
                FillCpuStatMessage(msg, curr, &it->second);
            }
            // 更新缓存（首次也会插入）
            cpu_stats_[cpu_name] = curr;
            args.clear();
        }
    }

    void CpuStatMonitor::Stop()
    {
        // 无资源需要释放
    }
}