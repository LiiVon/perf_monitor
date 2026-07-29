#pragma once

#include "monitor/monitor_inter.h"
#include "monitor_info.pb.h"
#include <string>
#include <unordered_map>
#include <chrono>

namespace monitor
{
    class DiskMonitor : public MonitorInter
    {
    public:
        DiskMonitor() = default;
        virtual ~DiskMonitor() = default;
        void UpdateOnce(monitor::proto::MonitorInfo *monitor_info) override;
        void Stop() override;

    private:
        struct DiskSample
        {
            uint64_t reads;
            uint64_t writes;
            uint64_t sectors_read;
            uint64_t sectors_written;

            uint64_t read_time_ms;
            uint64_t write_time_ms;
            uint64_t io_in_progress;
            uint64_t io_time_ms;
            uint64_t weighted_io_time_ms;
        };
    private:
        std::unordered_map<std::string, DiskSample> last_samples;
        std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_time;
    };
} // namespace monitor