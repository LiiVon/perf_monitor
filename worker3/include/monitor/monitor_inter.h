#pragma once 

#include <string>
#include "monitor_info.pb.h"

namespace monitor
{
    // 监控器接口
    class MonitorInter
    {
    public:
        MonitorInter() = default;
        virtual ~MonitorInter() = default;

        // 指向 MonitorInfo 对象的指针，该对象由上层管理（比如 MetricCollector 创建），各监控器将自己的数据写入其中。
        virtual void UpdateOnce(monitor::proto::MonitorInfo* monitor_info) = 0;
        
        // 优雅停止
        virtual void Stop() = 0;
    };
}