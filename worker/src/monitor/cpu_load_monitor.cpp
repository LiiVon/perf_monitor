#include "monitor/cpu_load_monitor.h"
#include "monitor/monitor_structs.h"
#include "monitor_info.grpc.pb.h"
#include "monitor_info.pb.h"
#include "utils/read_file.h"
#include <cstdio>
#include <string>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace monitor
{
    // 从 /proc/loadavg 读取负载信息作为后备方案
    static bool ReadLoadFromProc(float *load1, float *load5, float *load15)
    {
        ReadFile rf("/proc/loadavg");
        std::vector<std::string> fields;
        if (rf.ReadLine(&fields))
        {
            if (fields.size() >= 3)
            {
                *load1 = std::stof(fields[0]);
                *load5 = std::stof(fields[1]);
                *load15 = std::stof(fields[2]);
                return true;
            }
        }
        return false;
    }

    void CpuLoadMonitor::UpdateOnce(monitor::proto::MonitorInfo *monitor_info)
    {
        // 首先尝试从内核模块读取
        int fd = open("/dev/cpu_load_monitor", O_RDONLY);
        if (fd >= 0)
        {
            size_t load_size = sizeof(struct cpu_load);

            // 使用mmap映射内核模块的负载数据
            void *addr = mmap(nullptr, load_size, PROT_READ, MAP_SHARED, fd, 0);
            if (addr != MAP_FAILED)
            {
                struct cpu_load info;
                memcpy(&info, addr, load_size);

                // 写入 grpc消息体
                auto cpu_load_msg = monitor_info->mutable_cpu_load();
                cpu_load_msg->set_load_avg_1(info.load_avg_1);
                cpu_load_msg->set_load_avg_5(info.load_avg_5);
                cpu_load_msg->set_load_avg_15(info.load_avg_15);

                munmap(addr, load_size);
                close(fd);
                return;
            }
        }

        // 如果无法从内核模块读取，尝试从 /proc/loadavg 读取
        float load1, load5, load15;
        if (ReadLoadFromProc(&load1, &load5, &load15))
        {
            // 写入 grpc消息体
            auto cpu_load_msg = monitor_info->mutable_cpu_load();
            cpu_load_msg->set_load_avg_1(load1);
            cpu_load_msg->set_load_avg_5(load5);
            cpu_load_msg->set_load_avg_15(load15);
        }
    }


    void CpuLoadMonitor::Stop()
    {
        // 这里可以实现一些清理工作，如果有需要的话
    }
}