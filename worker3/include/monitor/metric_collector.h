#pragma once

#include "monitor/monitor_inter.h"
#include "monitor_info.pb.h"
#include <memory>
#include <string>
#include <vector>

namespace monitor
{
    class MetricCollector
    {
    public:
        MetricCollector();
        virtual ~MetricCollector();

        void CollectAll(monitor::proto::MonitorInfo* monitor_info);

    private:
        std::vector<std::unique_ptr<MonitorInter>> monitors_;
        std::string hostname_;
    };
}
