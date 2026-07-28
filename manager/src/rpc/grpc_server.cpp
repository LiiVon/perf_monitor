#include "rpc/grpc_server.h"
#include <iostream>

namespace monitor
{
    GrpcServerImpl::GrpcServerImpl()
        : running_(true)
    {
        // 启动后台工作线程
        worker_thread_ = std::thread(&GrpcServerImpl::WorkerThread, this);
    }

    GrpcServerImpl::~GrpcServerImpl()
    {
        // 停止后台线程
        running_ = false;
        queue_cv_.notify_all();
        if (worker_thread_.joinable())
        {
            worker_thread_.join();
        }
    }

    // 接收工作者推送的监控数据 Worker 调用它时，会传入监控数据。
    ::grpc::Status GrpcServerImpl::SetMonitorInfo(::grpc::ServerContext *context,
                                                  const ::monitor::proto::MonitorInfo *request,
                                                  ::google::protobuf::Empty *response)
    {
        if (!request)
        {
            return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, "Empty request");
        }

        // 获取主机名
        std::string host_name = request->name();
        if (host_name.empty() && request->has_host_info())
        {
            host_name = request->host_info().hostname();
        }

        if (host_name.empty())
        {
            return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, "Missing hostname");
        }

        // 存储数据到缓存 含内存淘汰
        StoreData(host_name, *request);

        // 异步执行回调（不阻塞 RPC 响应）
        if (callback_)
        {
            CallbackTask task;
            task.info = *request;

            {
                std::lock_guard<std::mutex> lock(queue_mtx_);
                task_queue_.push(std::move(task));
            }
            queue_cv_.notify_one();
        }
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

    bool GrpcServerImpl::GetHostData(const std::string &host_name, HostData *data)
    {
        if (!data)
            return false;

        std::lock_guard<std::mutex> lock(mtx_);
        auto it = host_data_.find(host_name);
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

    void GrpcServerImpl::StoreData(const std::string &hostname, const monitor::proto::MonitorInfo &info)
    {
        auto now = std::chrono::steady_clock::now();

        {
            std::lock_guard<std::mutex> lock(mtx_);

            auto it = host_data_.find(hostname);
            if (it != host_data_.end())
            {
                // 更新已有主机的数据和时间戳
                it->second.monitor_info = info;
                it->second.timestamp = now;

                // 更新 LRU 列表（将该主机移到列表前端）
                for (auto list_it = lru_list_.begin(); list_it != lru_list_.end(); ++list_it)
                {
                    if (list_it->first == hostname)
                    {
                        lru_list_.erase(list_it);
                        break;
                    }
                }
            }
            else
            {
                // 新主机，先检查是否需要淘汰
                EvictIfNeeded();

                // 添加新主机的数据和时间戳
                HostData host_data;
                host_data.monitor_info = info;
                host_data.timestamp = now;
                host_data_[hostname] = std::move(host_data);
                lru_list_.emplace_back(hostname, now);
            }
        }
    }

    void GrpcServerImpl::EvictIfNeeded()
    {
        auto last = lru_list_.back();
        std::string oldest_host = last.first;

        // 从缓存中移除最旧的主机数据
        host_data_.erase(oldest_host);
        lru_list_.pop_back();

        std::cout << "[GrpcServerImpl] Evicted oldest host: " << oldest_host
                  << " (cache size: " << host_data_.size() << ")" << std::endl;
    }

    void GrpcServerImpl::WorkerThread()
    {
        while (running_)
        {
            CallbackTask task;

            {
                std::unique_lock<std::mutex> lock(queue_mtx_);
                queue_cv_.wait(lock, [this]
                               { return !task_queue_.empty() || !running_; });

                if (!running_ && task_queue_.empty())
                {
                    break; // 退出线程
                }

                if (!task_queue_.empty())
                {
                    task = std::move(task_queue_.front());
                    task_queue_.pop();
                }
                else
                {
                    continue; // 如果队列为空，继续等待
                }
            }

            // 执行回调
            if (callback_)
            {
                try
                {
                    callback_(task.info);
                }
                catch (const std::exception &e)
                {
                    std::cerr << "[GrpcServerImpl] Callback exception: " << e.what() << std::endl;
                }
                catch (...)
                {
                    std::cerr << "[GrpcServerImpl] Callback unknown exception" << std::endl;
                }
            }
        }
    }
}
