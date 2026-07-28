#pragma once

#include "monitor/metric_collector.h"
#include "monitor_info.grpc.pb.h"
#include "monitor_info.pb.h"
#include <grpcpp/grpcpp.h>
#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace monitor
{
    class MonitorPusher
    {
    public:
        // 管理端地址  间隔时间默认10s
        explicit MonitorPusher(const std::string &manager_addr, int interval_seconds = 10);
        ~MonitorPusher();

        // 启动推送线程
        void Start();

        // 停止推送线程
        void Stop();

        // 获取管理者地址
        const std::string &GetManagerAddress() const;
    private:
        void PushLoop();
        bool PushOnce();

        std::string manager_addr_;
        int interval_seconds_;
        std::atomic<bool> running_;
        std::unique_ptr<std::thread> thread_;
        std::unique_ptr<MetricCollector> collector_;
        std::unique_ptr<monitor::proto::GrpcManager::Stub> stub_;
    };
}
