#include "monitor/cpu_softirq_monitor.h"
#include "monitor/monitor_structs.h"
#include "utils/read_file.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <chrono>
#include <cstring>
#include <iostream>

namespace monitor
{
    static const char *CPU_SOFTIRQ_DEVICE = "/dev/cpu_softirq_monitor";
    static constexpr size_t MAX_CPUS = 256;

    void CpuSoftIrqMonitor::UpdateOnce(monitor::proto::MonitorInfo *monitor_info)
    {
        int fd = open(CPU_SOFTIRQ_DEVICE, O_RDONLY);
        if (fd < 0)
        {
            // 设备不存在，可能内核模块未加载
            return;
        }

        size_t map_size = sizeof(struct softirq_stat) * MAX_CPUS;
        void *addr = mmap(nullptr, map_size, PROT_READ, MAP_SHARED, fd, 0);
        if (addr == MAP_FAILED)
        {
            close(fd);
            return;
        }

        struct softirq_stat *stats = static_cast<struct softirq_stat *>(addr);
        auto now = std::chrono::steady_clock::now();

        // 遍历所有 CPU 的软中断统计数据
        for (size_t i = 0; i < MAX_CPUS; ++i)
        {
            // 检查是否到达数据末尾
            if (stats[i].cpu_name[0] == '\0')
            {
                break;
            }

            std::string cpu_name(stats[i].cpu_name);

            // 查找之前的采样数据
            auto it = cpu_softirqs_.find(cpu_name);

            // 创建 protobuf 消息
            auto *softirq_msg = monitor_info->add_soft_irq();
            softirq_msg->set_cpu(cpu_name);

            if (it != cpu_softirqs_.end())
            {
                // 计算软中断时间差
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    now - it->second.timepoint)
                                    .count();
                double seconds = duration / 1000.0;

                if (seconds > 0)
                {
                    softirq_msg->set_hi((stats[i].hi - it->second.hi) / seconds);
                    softirq_msg->set_timer((stats[i].timer - it->second.timer) / seconds);
                    softirq_msg->set_net_tx((stats[i].net_tx - it->second.net_tx) / seconds);
                    softirq_msg->set_net_rx((stats[i].net_rx - it->second.net_rx) / seconds);
                    softirq_msg->set_block((stats[i].block - it->second.block) / seconds);
                    softirq_msg->set_irq_poll((stats[i].irq_poll - it->second.irq_poll) / seconds);
                    softirq_msg->set_tasklet((stats[i].tasklet - it->second.tasklet) / seconds);
                    softirq_msg->set_sched((stats[i].sched - it->second.sched) / seconds);
                    softirq_msg->set_hrtimer((stats[i].hrtimer - it->second.hrtimer) / seconds);
                    softirq_msg->set_rcu((stats[i].rcu - it->second.rcu) / seconds);
                }
                else
                {
                    // 时间差为 0，使用原始值
                    softirq_msg->set_hi(stats[i].hi);
                    softirq_msg->set_timer(stats[i].timer);
                    softirq_msg->set_net_tx(stats[i].net_tx);
                    softirq_msg->set_net_rx(stats[i].net_rx);
                    softirq_msg->set_block(stats[i].block);
                    softirq_msg->set_irq_poll(stats[i].irq_poll);
                    softirq_msg->set_tasklet(stats[i].tasklet);
                    softirq_msg->set_sched(stats[i].sched);
                    softirq_msg->set_hrtimer(stats[i].hrtimer);
                    softirq_msg->set_rcu(stats[i].rcu);
                }
            }
            else
            {
                // 首次采集，使用原始累计值
                softirq_msg->set_hi(stats[i].hi);
                softirq_msg->set_timer(stats[i].timer);
                softirq_msg->set_net_tx(stats[i].net_tx);
                softirq_msg->set_net_rx(stats[i].net_rx);
                softirq_msg->set_block(stats[i].block);
                softirq_msg->set_irq_poll(stats[i].irq_poll);
                softirq_msg->set_tasklet(stats[i].tasklet);
                softirq_msg->set_sched(stats[i].sched);
                softirq_msg->set_hrtimer(stats[i].hrtimer);
                softirq_msg->set_rcu(stats[i].rcu);
            }

            // 保存当前采样数据用于下次计算
            SoftIrq &cached = cpu_softirqs_[cpu_name];
            cached.cpu_name = cpu_name;
            cached.hi = stats[i].hi;
            cached.timer = stats[i].timer;
            cached.net_tx = stats[i].net_tx;
            cached.net_rx = stats[i].net_rx;
            cached.block = stats[i].block;
            cached.irq_poll = stats[i].irq_poll;
            cached.tasklet = stats[i].tasklet;
            cached.sched = stats[i].sched;
            cached.hrtimer = stats[i].hrtimer;
            cached.rcu = stats[i].rcu;
            cached.timepoint = now;
        }

        munmap(addr, map_size);
        close(fd);
    }


    void CpuSoftIrqMonitor::Stop()
    {
        // 这里可以实现一些清理工作，如果有需要的话
    }
}