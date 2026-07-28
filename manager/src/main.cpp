#include "monitor_info.grpc.pb.h"
#include "monitor_info.pb.h"
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>

using google::protobuf::Empty;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using monitor::proto::GrpcManager;
using monitor::proto::MonitorInfo;

class ManagerServiceImpl final : public GrpcManager::Service
{
    Status SetMonitorInfo(ServerContext *context, const MonitorInfo *request, Empty *response) override
    {
        std::cout << "=== Received data ===" << std::endl;
        std::cout << "Hostname: " << request->name() << std::endl;

        if (request->has_cpu_load())
        {
            auto &load = request->cpu_load();
            std::cout << "CPU Load: 1min=" << load.load_avg_1()
                      << " 5min=" << load.load_avg_5()
                      << " 15min=" << load.load_avg_15() << std::endl;
        }
        if (request->has_mem_info())
        {
            auto &mem = request->mem_info();
            std::cout << "Memory: used=" << mem.used_percent() << "%"
                      << " total=" << mem.total() << "GB"
                      << " avail=" << mem.avail() << "GB" << std::endl;
        }

        // 打印每个 CPU 核心统计
        for (int i = 0; i < request->cpu_stat_size(); ++i)
        {
            const auto &stat = request->cpu_stat(i);
            std::cout << "CPU " << stat.cpu_name() << ": "
                      << "usr=" << stat.usr_percent() << "% "
                      << "sys=" << stat.system_percent() << "% "
                      << "idle=" << stat.idle_percent() << "%" << std::endl;
        }

        // 打印软中断（前几个即可）
        for (int i = 0; i < request->soft_irq_size() && i < 3; ++i)
        {
            const auto &irq = request->soft_irq(i);
            std::cout << "SoftIRQ " << irq.cpu() << ": "
                      << "timer=" << irq.timer() << " "
                      << "net_rx=" << irq.net_rx() << std::endl;
        }
        std::cout << "Disk count: " << request->disk_info_size() << std::endl;
        std::cout << "Net count: " << request->net_info_size() << std::endl;
        if (request->has_host_info())
        {
            std::cout << "Host IP: " << request->host_info().ip_address() << std::endl;
        }
        std::cout << "=========================" << std::endl;
        return Status::OK;
    }

    Status GetMonitorInfo(ServerContext *context, const Empty *request, MonitorInfo *response) override
    {
        // 可以返回空，或者缓存最近的数据
        return Status::OK;
    }
};

int main()
{
    std::string server_address("0.0.0.0:50051");
    ManagerServiceImpl service;
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Manager listening on " << server_address << std::endl;
    server->Wait();
    return 0;
}