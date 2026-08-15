#include "rpc/grpc_server.h"
#include <iostream>

namespace monitor
{

    GrpcServerImpl::GrpcServerImpl()
    {
        running_ = true;
        worker_thread_ = std::thread(&GrpcServerImpl::WorkerThread, this);
    }

    GrpcServerImpl::~GrpcServerImpl()
    {
        // 停止工作线程
        {
            std::lock_guard<std::mutex> lock(queue_mtx_);
            running_ = false;
        }
        queue_cv_.notify_one();
        if (worker_thread_.joinable())
            worker_thread_.join();
    }

    ::grpc::Status GrpcServerImpl::SetMonitorInfo(
        ::grpc::ServerContext *context,
        const ::monitor::proto::MonitorInfo *request,
        ::google::protobuf::Empty *response)
    {
        if (!request)
        {
            return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                                  "Empty request");
        }

        // 获取主机名
        std::string hostname = request->name();
        if (hostname.empty() && request->has_host_info())
        {
            hostname = request->host_info().hostname();
        }

        if (hostname.empty())
        {
            return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                                  "Missing hostname");
        }

        auto now = std::chrono::steady_clock::now();

        // 存储数据到内存缓存 + 更新 LRU
        {
            std::lock_guard<std::mutex> lock(mtx_);
            HostData data;
            data.monitor_info = *request;
            data.timestamp = now;
            host_data_[hostname] = std::move(data);

            // 更新 LRU 列表：移到最前面
            for (auto it = lru_list_.begin(); it != lru_list_.end(); ++it)
            {
                if (it->first == hostname)
                {
                    lru_list_.erase(it);
                    break;
                }
            }
            lru_list_.emplace_front(hostname, now);

            // 内存淘汰
            EvictIfNeeded();
        }

        std::cout << "[GrpcServerImpl] Received data from: " << hostname << std::endl;

        // 异步入队，不阻塞 gRPC 工作线程
        {
            std::lock_guard<std::mutex> lock(queue_mtx_);
            task_queue_.push(CallbackTask{*request});
        }
        queue_cv_.notify_one();

        return ::grpc::Status::OK;
    }

    void GrpcServerImpl::SetDataReceivedCallback(DataReceivedCallback callback)
    {
        std::lock_guard<std::mutex> lock(callback_mtx_);
        callback_ = std::move(callback);
    }

    std::unordered_map<std::string, HostData> GrpcServerImpl::GetAllHostData()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return host_data_;
    }

    bool GrpcServerImpl::GetHostData(const std::string &hostname, HostData *data)
    {
        if (!data)
            return false;

        std::lock_guard<std::mutex> lock(mtx_);
        auto it = host_data_.find(hostname);
        if (it != host_data_.end())
        {
            *data = it->second;
            return true;
        }
        return false;
    }

    size_t GrpcServerImpl::GetHostCount()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return host_data_.size();
    }

    // ==================== 异步任务队列 ====================

    void GrpcServerImpl::WorkerThread()
    {
        while (true)
        {
            CallbackTask task;
            {
                std::unique_lock<std::mutex> lock(queue_mtx_);
                queue_cv_.wait(lock, [this] {
                    return !running_ || !task_queue_.empty();
                });

                if (!running_ && task_queue_.empty())
                    return;

                task = std::move(task_queue_.front());
                task_queue_.pop();
            }

            // 在后台线程执行回调（可能包含 MySQL 写入等耗时操作）
            DataReceivedCallback cb;
            {
                std::lock_guard<std::mutex> lock(callback_mtx_);
                cb = callback_;
            }
            if (cb)
            {
                cb(task.info);
            }
        }
    }

    void GrpcServerImpl::EvictIfNeeded()
    {
        // 调用方已持有 mtx_
        while (host_data_.size() > max_hosts_ && !lru_list_.empty())
        {
            auto &oldest = lru_list_.back();
            std::cout << "[GrpcServerImpl] Evicting stale host: "
                      << oldest.first << std::endl;
            host_data_.erase(oldest.first);
            lru_list_.pop_back();
        }
    }

} // namespace monitor