#include "rpc/grpc_server.h"
#include <iostream>

namespace monitor
{

// 构造函数 - 无需额外初始化
GrpcServerImpl::GrpcServerImpl() = default;

// 析构函数 - 无需清理资源
GrpcServerImpl::~GrpcServerImpl() = default;

// 接收工作者推送的监控数据
::grpc::Status GrpcServerImpl::SetMonitorInfo(
    ::grpc::ServerContext* context,
    const ::monitor::proto::MonitorInfo* request,
    ::google::protobuf::Empty* response)
{
    if (!request) {
        return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                              "Empty request");
    }

    // 获取主机名
    std::string hostname = request->name();
    if (hostname.empty() && request->has_host_info()) {
        hostname = request->host_info().hostname();
    }

    if (hostname.empty()) {
        return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                              "Missing hostname");
    }

    // 存储数据到内存缓存
    {
        std::lock_guard<std::mutex> lock(mtx_);
        HostData data;
        data.monitor_info = *request;
        data.timestamp = std::chrono::steady_clock::now();
        host_data_[hostname] = std::move(data);
    }

    // 打印接收日志（方便调试）
    std::cout << "[GrpcServerImpl] Received data from: " << hostname << std::endl;

    // 同步调用回调（如果已注册）
    // 注意：回调中不要做耗时操作，否则会阻塞 gRPC 工作线程
    if (callback_) {
        callback_(*request);
    }

    return ::grpc::Status::OK;
}

// 设置数据接收回调（线程安全）
void GrpcServerImpl::SetDataReceivedCallback(DataReceivedCallback callback)
{
    std::lock_guard<std::mutex> lock(callback_mtx_);
    callback_ = std::move(callback);
}

// 获取所有主机的缓存数据
std::unordered_map<std::string, HostData> GrpcServerImpl::GetAllHostData()
{
    std::lock_guard<std::mutex> lock(mtx_);
    return host_data_;   // 返回副本
}

// 获取指定主机的缓存数据
bool GrpcServerImpl::GetHostData(const std::string& hostname, HostData* data)
{
    if (!data) return false;

    std::lock_guard<std::mutex> lock(mtx_);
    auto it = host_data_.find(hostname);
    if (it != host_data_.end()) {
        *data = it->second;
        return true;
    }
    return false;
}

// 获取当前缓存的主机数量
size_t GrpcServerImpl::GetHostCount()
{
    std::lock_guard<std::mutex> lock(mtx_);
    return host_data_.size();
}

} // namespace monitor