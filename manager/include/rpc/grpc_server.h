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
        virtual ~GrpcServerImpl();

        // 接收工作者推送的监控数据 Worker 调用它时，会传入监控数据。
        ::grpc::Status
        SetMonitorInfo(::grpc::ServerContext *context,
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
        // // 异步任务
        // struct CallbackTask
        // {
        //     monitor::proto::MonitorInfo info;
        // };

        // // 后台工作线程函数
        // void WorkerThread();

        // // 内存淘汰逻辑
        // //  如果只是无限制往 unordered_map 里存，遇到恶意伪造海量主机名，程序内存就会无限增长导致崩溃
        // void EvictIfNeeded();

        // // 存储数据到缓存
        // void StoreData(const std::string &hostname, const monitor::proto::MonitorInfo &info);

    private:
        std::mutex mtx_;          // 保护 host_data_ 的互斥锁
        std::mutex callback_mtx_; // 保护回调函数的设置
        // 存储每个主机的监控数据和时间戳
        std::unordered_map<std::string, HostData> host_data_;

        DataReceivedCallback callback_;

        // // 用于 LRU 淘汰的有序列表（记录主机名和更新时间）
        // // 记录了所有主机最近一次上报的时间顺序。最新上报的放在最前面，最久没上报的放在最后面。
        // std::list<std::pair<std::string, std::chrono::steady_clock::time_point>> lru_list_;
        // size_t max_hosts_ = 1000; // 最大缓存主机数，可配置

        // // 回调相关
        //

        // // 异步任务队列
        // std::queue<CallbackTask> task_queue_;
        // std::mutex queue_mtx_;
        // std::condition_variable queue_cv_;
        // std::atomic<bool> running_;
        // std::thread worker_thread_;
    };
}

/*
Worker 调用 SetMonitorInfo
        │
        ▼
    (主线程 - 极快)
 1. 存到 host_data_ (更新仓库)
 2. 构造 CallbackTask 放入 task_queue_ (写好待办)
 3. 按门铃 queue_cv_.notify_one()
 4. 立即 return Status::OK (毫秒级响应，不卡住 Worker)
        │
        │ ========== 异步分割线 ==========
        │
        ▼
  (后台线程 - 慢速处理)
 5. worker_thread_ 被门铃唤醒
 6. 从队列取出 CallbackTask
 7. 执行 callback_(info) (比如写 MySQL 数据库，即使耗时 1 秒也不影响主 RPC)
*/