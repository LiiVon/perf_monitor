#pragma once 

#include "monitor/monitor_inter.h"
#include <string>

namespace monitor
{
    class UserMonitor : public MonitorInter
    {
    public:
        UserMonitor() = default;
        virtual ~UserMonitor() = default;

        void UpdateOnce(monitor::proto::MonitorInfo *monitor_info) override;
        void Stop() override;

    private:
        std::string GetUsernameByUid(uid_t uid);
    };
}