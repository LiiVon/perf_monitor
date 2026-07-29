#pragma once

#include <chrono>

namespace monitor
{
    // 计算两个时间点之间的时间差，并以“秒”为单位返回
    class Utils
    {
    public:
        static double SteadyTimeSecond(
            const std::chrono::steady_clock::time_point &t1,
            const std::chrono::steady_clock::time_point &t2)
        {

            std::chrono::duration<double> time_second = t1 - t2;
            return time_second.count();
        }
    };
}