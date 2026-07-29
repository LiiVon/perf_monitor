#include "monitor/disk_monitor.h"
#include "utils/read_file.h"
#include <ctime>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <chrono>

namespace monitor
{
    
    static const std::string DISK_STATS_FILE = "/proc/diskstats";

    void DiskMonitor::UpdateOnce(monitor::proto::MonitorInfo *monitor_info)
    {
        ReadFile rf(DISK_STATS_FILE);
        std::vector<std::string> args;
        auto now = std::chrono::steady_clock::now();

        while (true)
        {
            args.clear();
            if (!rf.ReadLine(&args))
            {
                break;
            }

            if (args.size() < 14)
            {
                continue;
            }
            std::string device_name = args[2];

            // 跳过虚拟盘
            if(device_name.find("loop") == 0 || device_name.find("ram") == 0)
            {
                continue;
            }

            DiskSample current_sample;
            current_sample.reads = std::stoull(args[3]);
            current_sample.writes = std::stoull(args[7]);
            current_sample.sectors_read = std::stoull(args[5]);
            current_sample.sectors_written = std::stoull(args[9]);
            current_sample.read_time_ms = std::stoull(args[6]);
            current_sample.write_time_ms = std::stoull(args[10]);
            current_sample.io_in_progress = std::stoull(args[11]);
            current_sample.io_time_ms = std::stoull(args[12]);
            current_sample.weighted_io_time_ms = std::stoull(args[13]);

            auto *disk = monitor_info->add_disk_info();
            disk->set_name(device_name);
            disk->set_reads(current_sample.reads);
            disk->set_writes(current_sample.writes);
            disk->set_sectors_read(current_sample.sectors_read);
            disk->set_sectors_written(current_sample.sectors_written);
            disk->set_read_time_ms(current_sample.read_time_ms);
            disk->set_write_time_ms(current_sample.write_time_ms);
            disk->set_io_in_progress(current_sample.io_in_progress);
            disk->set_io_time_ms(current_sample.io_time_ms);
            disk->set_weighted_io_time_ms(current_sample.weighted_io_time_ms);

            // 计算速率
            auto it = last_samples.find(device_name);
            double dt = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time[device_name]).count() / 1000.0;
            if (it != last_samples.end() && dt > 0)
            {
                const DiskSample &last_sample = it->second;
                double read_ios = static_cast<double>(current_sample.reads - last_sample.reads);
                double write_ios = static_cast<double>(current_sample.writes - last_sample.writes);
                double read_bytes = static_cast<double>(current_sample.sectors_read - last_sample.sectors_read) * 512.0;
                double write_bytes = static_cast<double>(current_sample.sectors_written - last_sample.sectors_written) * 512.0;
                double read_time = static_cast<double>(current_sample.read_time_ms - last_sample.read_time_ms);
                double write_time = static_cast<double>(current_sample.write_time_ms - last_sample.write_time_ms);
                double io_time = static_cast<double>(current_sample.io_time_ms - last_sample.io_time_ms);

                disk->set_write_bytes_per_sec(write_bytes / dt);
                disk->set_read_bytes_per_sec(read_bytes / dt);
                disk->set_read_iops(read_ios / dt);
                disk->set_write_iops(write_ios / dt);
                disk->set_avg_read_latency_ms(read_ios > 0 ? read_time / read_ios : 0.0);
                disk->set_avg_write_latency_ms(write_ios > 0 ? write_time / write_ios : 0.0);
                disk->set_util_percent(io_time / (dt * 10.0)); // 将util_percent转换为百分比
            }
            else 
            {
                // 第一次采样，无法计算速率
                disk->set_write_bytes_per_sec(0.0);
                disk->set_read_bytes_per_sec(0.0);
                disk->set_read_iops(0.0);
                disk->set_write_iops(0.0);
                disk->set_avg_read_latency_ms(0.0);
                disk->set_avg_write_latency_ms(0.0);
                disk->set_util_percent(0.0);
            }
            // 更新缓存
            last_samples[device_name] = current_sample;
            last_time[device_name] = now;
        }
    }

    void DiskMonitor::Stop()
    {
    }
}