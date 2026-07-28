#pragma once

#include "monitor_info.pb.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace monitor
{
    // HostScore 结构体用于存储主机的监控信息、评分和时间戳
    struct HostScore
    {
        monitor::proto::MonitorInfo monitor_info;
        double score;
        std::chrono::steady_clock::time_point timestamp;
    };

    // 管理多个远程主机的监控数据（推送模式）
    class HostManager
    {
    public:
        HostManager() = default;
        ~HostManager() = default;

        // 启动后台处理线程
        void Start();
        void stop();

        // 接收工作者推送的数据（由 gRPC 服务调用）
        void OnDataReceived(const monitor::proto::MonitorInfo &monitor_info);

        // 获取所有主机的监控数据和评分
        std::unordered_map<std::string, HostScore> GetAllHostScores();

        // 获取最优主机
        std::string GetBestHost();

    private:
        // 后台处理循环
        void ProcessLoop();

        // 计算主机评分
        double CalcScore(const monitor::proto::MonitorInfo &monitor_info);

        void WriteToMysql(const std::string &host_name, const HostScore &host_score,
                          double net_in_rate, double net_out_rate,
                          float cpu_percent_rate, float usr_percent_rate,
                          float system_percent_rate, float nice_percent_rate,
                          float idle_percent_rate, float io_wait_percent_rate,
                          float irq_percent_rate, float soft_irq_percent_rate,
                          float steal_percent_rate, float guest_percent_rate,
                          float guest_nice_percent_rate, float load_avg_1_rate,
                          float load_avg_3_rate, float load_avg_15_rate,
                          float mem_used_percent_rate, float mem_total_rate,
                          float mem_free_rate, float mem_avail_rate,
                          float net_in_rate_rate, float net_out_rate_rate,
                          float net_in_drop_rate_rate, float net_out_drop_rate_rate);

        std::unordered_map<std::string, HostScore> host_scores_;
        std::mutex mtx_;
        std::atomic<bool> running_;
        std::unique_ptr<std::thread> thread_;
    };
}