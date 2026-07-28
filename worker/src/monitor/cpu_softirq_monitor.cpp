#include "monitor/cpu_softirq_monitor.h"
#include "monitor/monitor_structs.h"
#include "utils/read_file.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <chrono>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>
#include <unordered_map>

namespace monitor
{
    static const char *CPU_SOFTIRQ_DEVICE = "/dev/cpu_softirq_monitor";
    static const char *PROC_SOFTIRQ_PATH = "/proc/softirqs";
    static constexpr size_t MAX_CPUS = 256;

    // 从 /proc/softirqs 读取当前软中断计数，返回 CPU 名到 SoftIrq 的映射
    static std::unordered_map<std::string, SoftIrq> ReadSoftIrqFromProc()
    {
        std::unordered_map<std::string, SoftIrq> result;
        ReadFile rf(PROC_SOFTIRQ_PATH);
        std::vector<std::string> args;

        // 读取标题行（格式："          CPU0       CPU1       CPU2 ..."）
        if (!rf.ReadLine(&args))
            return result;
        std::vector<std::string> cpu_names;
        for (const auto &token : args)
        {
            if (!token.empty() && token.find("CPU") == 0)
            {
                cpu_names.push_back(token);
            }
        }
        if (cpu_names.empty())
            return result;

        // 逐行读取每种软中断
        while (rf.ReadLine(&args))
        {
            if (args.size() < 2)
                continue;
            std::string name = args[0];
            if (name.back() == ':')
                name.pop_back();
            if (name.empty())
                continue;

            // 解析所有 CPU 的计数值
            std::vector<uint64_t> counts;
            for (size_t i = 1; i < args.size() && i <= cpu_names.size(); ++i)
            {
                try
                {
                    counts.push_back(std::stoull(args[i]));
                }
                catch (...)
                {
                    counts.push_back(0);
                }
            }
            while (counts.size() < cpu_names.size())
                counts.push_back(0);

            // 将值填入对应 CPU 的 SoftIrq 结构
            for (size_t j = 0; j < cpu_names.size(); ++j)
            {
                const std::string &cpu = cpu_names[j];
                auto &entry = result[cpu];
                entry.cpu_name = cpu;
                if (name == "HI")
                    entry.hi = counts[j];
                else if (name == "TIMER")
                    entry.timer = counts[j];
                else if (name == "NET_TX")
                    entry.net_tx = counts[j];
                else if (name == "NET_RX")
                    entry.net_rx = counts[j];
                else if (name == "BLOCK")
                    entry.block = counts[j];
                else if (name == "IRQ_POLL")
                    entry.irq_poll = counts[j];
                else if (name == "TASKLET")
                    entry.tasklet = counts[j];
                else if (name == "SCHED")
                    entry.sched = counts[j];
                else if (name == "HRTIMER")
                    entry.hrtimer = counts[j];
                else if (name == "RCU")
                    entry.rcu = counts[j];
            }
        }
        return result;
    }

    // 填充单个 CPU 的软中断消息（复用计算逻辑）
    static void FillSoftIrqMessage(monitor::proto::SoftIrq *msg,
                                   const SoftIrq &curr,
                                   const SoftIrq *old,
                                   double seconds)
    {
        msg->set_cpu(curr.cpu_name);
        if (old && seconds > 0)
        {
            msg->set_hi((curr.hi - old->hi) / seconds);
            msg->set_timer((curr.timer - old->timer) / seconds);
            msg->set_net_tx((curr.net_tx - old->net_tx) / seconds);
            msg->set_net_rx((curr.net_rx - old->net_rx) / seconds);
            msg->set_block((curr.block - old->block) / seconds);
            msg->set_irq_poll((curr.irq_poll - old->irq_poll) / seconds);
            msg->set_tasklet((curr.tasklet - old->tasklet) / seconds);
            msg->set_sched((curr.sched - old->sched) / seconds);
            msg->set_hrtimer((curr.hrtimer - old->hrtimer) / seconds);
            msg->set_rcu((curr.rcu - old->rcu) / seconds);
        }
        else
        {
            // 首次或时间差无效，直接填累计值
            msg->set_hi(curr.hi);
            msg->set_timer(curr.timer);
            msg->set_net_tx(curr.net_tx);
            msg->set_net_rx(curr.net_rx);
            msg->set_block(curr.block);
            msg->set_irq_poll(curr.irq_poll);
            msg->set_tasklet(curr.tasklet);
            msg->set_sched(curr.sched);
            msg->set_hrtimer(curr.hrtimer);
            msg->set_rcu(curr.rcu);
        }
    }

    void CpuSoftIrqMonitor::UpdateOnce(monitor::proto::MonitorInfo *monitor_info)
    {
        // ==================== 尝试内核模块 ====================
        int fd = open(CPU_SOFTIRQ_DEVICE, O_RDONLY);
        if (fd >= 0)
        {
            size_t map_size = sizeof(struct softirq_stat) * MAX_CPUS;
            void *addr = mmap(nullptr, map_size, PROT_READ, MAP_SHARED, fd, 0);
            if (addr != MAP_FAILED)
            {
                struct softirq_stat *stats = static_cast<struct softirq_stat *>(addr);
                auto now = std::chrono::steady_clock::now();

                for (size_t i = 0; i < MAX_CPUS; ++i)
                {
                    if (stats[i].cpu_name[0] == '\0')
                        break;
                    std::string cpu_name(stats[i].cpu_name);
                    auto it = cpu_softirqs_.find(cpu_name);
                    auto *softirq_msg = monitor_info->add_soft_irq();

                    // 构造当前 SoftIrq
                    SoftIrq curr;
                    curr.cpu_name = cpu_name;
                    curr.hi = stats[i].hi;
                    curr.timer = stats[i].timer;
                    curr.net_tx = stats[i].net_tx;
                    curr.net_rx = stats[i].net_rx;
                    curr.block = stats[i].block;
                    curr.irq_poll = stats[i].irq_poll;
                    curr.tasklet = stats[i].tasklet;
                    curr.sched = stats[i].sched;
                    curr.hrtimer = stats[i].hrtimer;
                    curr.rcu = stats[i].rcu;

                    double seconds = 0.0;
                    if (it != cpu_softirqs_.end())
                    {
                        seconds = std::chrono::duration<double>(now - it->second.timepoint).count();
                    }
                    FillSoftIrqMessage(softirq_msg, curr,
                                       (it != cpu_softirqs_.end() ? &it->second : nullptr),
                                       seconds);

                    // 更新缓存
                    curr.timepoint = now;
                    cpu_softirqs_[cpu_name] = curr;
                }

                munmap(addr, map_size);
                close(fd);
                return; // 内核模块成功，直接返回
            }
            close(fd);
        }

        
        // ==================== 回退：从 /proc/softirqs 读取 ====================
        auto current_data = ReadSoftIrqFromProc();
        if (current_data.empty())
            return;

        auto now = std::chrono::steady_clock::now();

        // 遍历每个 CPU
        for (auto &pair : current_data)
        {
            const std::string &cpu_name = pair.first;
            const SoftIrq &curr = pair.second;

            auto it = cpu_softirqs_.find(cpu_name);
            auto *softirq_msg = monitor_info->add_soft_irq();

            double seconds = 0.0;
            if (it != cpu_softirqs_.end())
            {
                seconds = std::chrono::duration<double>(now - it->second.timepoint).count();
            }
            FillSoftIrqMessage(softirq_msg, curr,
                               (it != cpu_softirqs_.end() ? &it->second : nullptr),
                               seconds);

            // 更新缓存
            SoftIrq cached = curr;
            cached.timepoint = now;
            cpu_softirqs_[cpu_name] = cached;
        }
    }

    void CpuSoftIrqMonitor::Stop()
    {
        // 无需特殊清理
    }
}