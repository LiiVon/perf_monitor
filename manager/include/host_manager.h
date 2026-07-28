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

// 连接“网络接收层”（GrpcServerImpl）和“数据存储层”（MySQL）的核心枢纽。
namespace monitor
{
    // 主机评分数据
    struct HostScore
    {
        monitor::proto::MonitorInfo monitor_info;        // 完整的监控数据
        double score;                                    // 计算出的综合评分（0~100）
        std::chrono::steady_clock::time_point timestamp; // 最后更新时间
    };

    // 所有变化率（用于 WriteToMysql）
    struct RateInfo
    {
        float cpu_percent = 0;
        float usr_percent = 0;
        float system_percent = 0;
        float nice_percent = 0;
        float idle_percent = 0;
        float io_wait_percent = 0;
        float irq_percent = 0;
        float soft_irq_percent = 0;
        float load_avg_1 = 0;
        float load_avg_5 = 0;
        float load_avg_15 = 0;
        float mem_used_percent = 0;
        float mem_total = 0;
        float mem_free = 0;
        float mem_avail = 0;
        float net_in_rate = 0;
        float net_out_rate = 0;
        // 以下字段未使用，但保留以防万一
        float net_in_drop_rate = 0;
        float net_out_drop_rate = 0;
    };

    // 管理多个远程主机的监控数据（推送模式）
    class HostManager
    {
    public:
        HostManager();
        ~HostManager();

        // 启动后台处理线程
        void Start();
        void Stop();

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

        // 写入 MySQL（参数已简化为 RateInfo）
        void WriteToMysql(const std::string &host_name,
                          const HostScore &host_score,
                          double net_in_rate,
                          double net_out_rate,
                          const RateInfo &rates);

        // 辅助：提取主机名
        static std::string ExtractHostName(const monitor::proto::MonitorInfo &info);

        // 辅助：提取性能数据（当前采样）
        static void ExtractPerfData(const monitor::proto::MonitorInfo &info,
                                    double net_in_rate,
                                    double net_out_rate,
                                    double score,
                                    struct PerfSample &curr);

        // 辅助：计算变化率
        static RateInfo ComputeRates(const struct PerfSample &curr,
                                     const struct PerfSample &last);

        std::unordered_map<std::string, HostScore> host_scores_;
        std::mutex mtx_;
        std::atomic<bool> running_;
        std::unique_ptr<std::thread> thread_;
    };
}

/*
Worker (采集)
    ↓ gRPC
GrpcServerImpl (接收)
    ↓ 回调
HostManager (业务处理)
    ├─ 内存缓存 (host_scores_) → 供查询
    ├─ MySQL (5张表) → 持久化
    └─ 后台线程 (ProcessLoop) → 清理离线主机
*/