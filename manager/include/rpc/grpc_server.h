#pragma once

#include "monitor_info.grpc.pb.h"
#include "monitor_info.pb.h"
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <list>
#include <queue>
#include <thread>
#include <condition_variable>
#include <atomic>

// gRPC 的管理端服务实现类，接收工作者推送的监控数据
namespace monitor
{
    // HostData 结构体用于存储主机的监控信息和时间戳
    struct HostData
    {
        monitor::proto::MonitorInfo monitor_info;
        std::chrono::steady_clock::time_point timestamp;
    };

    // 数据接收回调函数类型
    using DataReceivedCallback = std::function<void(const monitor::proto::MonitorInfo &)>;

    // gRPC 服务实现类 - 接收工作者推送的监控数据
    class GrpcServerImpl : public monitor::proto::GrpcManager::Service
    {
    public:
        GrpcServerImpl();
        ~GrpcServerImpl() override;

        // 接收工作者推送的监控数据
        ::grpc::Status SetMonitorInfo(
            ::grpc::ServerContext *context,
            const ::monitor::proto::MonitorInfo *request,
            ::google::protobuf::Empty *response) override;

        // 设置数据接收回调（线程安全）
        void SetDataReceivedCallback(DataReceivedCallback callback);

        // 获取所有主机的监控数据和时间戳
        std::unordered_map<std::string, HostData> GetAllHostData();

        // 获取指定主机的监控数据和时间戳
        bool GetHostData(const std::string &host_name, HostData *data);

        // 获取当前缓存的主机数量
        size_t GetHostCount();

    private:
        // ---- 异步任务队列（避免阻塞 gRPC 工作线程）----
        struct CallbackTask
        {
            monitor::proto::MonitorInfo info;
        };
        void WorkerThread();   // 后台消费线程
        void EvictIfNeeded();  // LRU 淘汰

        // ---- 内存缓存 ----
        std::mutex mtx_;  // 保护 host_data_
        std::unordered_map<std::string, HostData> host_data_;

        // ---- LRU 淘汰 ----
        // 最新上报的放前面，最久没上报的放后面
        std::list<std::pair<std::string, std::chrono::steady_clock::time_point>> lru_list_;
        size_t max_hosts_ = 1000;

        // ---- 回调（异步执行）----
        std::mutex callback_mtx_;
        DataReceivedCallback callback_;

        // ---- 异步队列 ----
        std::queue<CallbackTask> task_queue_;
        std::mutex queue_mtx_;
        std::condition_variable queue_cv_;
        std::atomic<bool> running_{false};
        std::thread worker_thread_;
    };
}