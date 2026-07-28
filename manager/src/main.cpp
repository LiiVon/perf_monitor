#include "monitor_info.grpc.pb.h"
#include "monitor_info.pb.h"
#include "rpc/grpc_server.h"
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>
#include <chrono>
#include <csignal>
#include <memory>
#include <thread>

using google::protobuf::Empty;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using monitor::proto::GrpcManager;
using monitor::proto::MonitorInfo;

/*
GrpcManager::Service：这是 protoc 根据你的 query_api.proto 自动生成的抽象基类。它里面声明了 SetMonitorInfo 和 GetMonitorInfo 两个纯虚函数（或虚函数）。
override：你的 ManagerServiceImpl 实现了这两个方法，填补了具体的业务逻辑。
当 Worker 发来 SetMonitorInfo 请求时，gRPC 框架会自动回调你的这个 SetMonitorInfo 方法。
*/
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

// int main()
// {
//     /*
//     server_address："0.0.0.0:50051" 表示监听本机所有网络接口（包括 127.0.0.1 和外网 IP）的 50051 端口。这样无论是本机的 Worker 还是其他机器的 Worker，都能连上来。
//     ServerBuilder：这是 gRPC 提供的“建筑工头”，用来组装你的服务端（配置监听端口、注册服务、设置认证等）。
//     service：你刚才写好的业务逻辑实例。
//     */
//     std::string server_address("0.0.0.0:50051");
//     ManagerServiceImpl service;
//     ServerBuilder builder;

//     /*
//     AddListeningPort：告诉 Builder 在哪个地址和端口开门营业。
//         grpc::InsecureServerCredentials()：使用明文传输（无加密）。与 Worker 端的 InsecureChannelCredentials 对应。
//     RegisterService：告诉 Builder，如果有快递（RPC 请求）来了，就找 service 这个对象来处理。
//     */
//     builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
//     builder.RegisterService(&service);

//     /*
//     BuildAndStart()：Builder 把房子建好，并立即开始监听端口。此时 Manager 已经准备好接收请求了。
//     server->Wait()：这是一个阻塞调用。它会卡住当前线程（main 函数），让程序一直运行，直到你按 Ctrl+C 或调用 server->Shutdown()。这是为了让服务端持续运行，而不是执行完 main 就退出。
//     */
//     std::unique_ptr<Server> server(builder.BuildAndStart());
//     std::cout << "Manager listening on " << server_address << std::endl;
//     server->Wait();
//     return 0;
// }

static bool running = true;
void SignalHandler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
    {
        std::cout << "Received exit signal, stopping..." << std::endl;
        running = false;
    }
}
int main(int argc, char *argv[])
{
    // 解析命令行参数：Manager 服务地址
    std::string server_address = "0.0.0.0:50051"; // 默认
    if (argc >= 2)
    {
        server_address = argv[1];
    }

    // 设置信号处理（Ctrl+C 优雅退出）
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    // 创建 gRPC 服务实例
    monitor::GrpcServerImpl service;

    // 测试回调
    service.SetDataReceivedCallback([](const monitor::proto::MonitorInfo &info)
                                    {
        std::cout << "[Callback] Received data for host: " << info.name() << std::endl;
        if(info.has_cpu_load())
        {
            std::cout << "[Callback] CPU Load: 1min=" << info.cpu_load().load_avg_1()
                      << " 5min=" << info.cpu_load().load_avg_5()
                      << " 15min=" << info.cpu_load().load_avg_15() << std::endl;
        }
        if(info.has_mem_info())
        {
            std::cout << "[Callback] Memory: used=" << info.mem_info().used_percent() << "%"
                      << " total=" << info.mem_info().total() << "GB"
                      << " avail=" << info.mem_info().avail() << "GB" << std::endl;
        } });

    // 启动 gRPC 服务
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    
    if(!server)
    {
        std::cerr << "Failed to start gRPC server on " << server_address << std::endl;
        return 1;
    }
    std::cout << "Manager listening on " << server_address << std::endl;
    std::cout << "Press Ctrl+C to stop the server." << std::endl;

    while(running)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "Shutting down gRPC server..." << std::endl;
    server->Shutdown();
    std::cout << "Server stopped." << std::endl;

    return 0;
}