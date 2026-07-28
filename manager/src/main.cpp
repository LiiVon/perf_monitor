#include "host_manager.h"
#include "query_manager.h"
#include "rpc/grpc_server.h"
#include "rpc/query_service.h"

#include <grpc/grpc.h>
#include <grpcpp/server_builder.h>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>


constexpr char kDefaultListenAddress[] = "0.0.0.0:50051";
constexpr char kDefaultMysqlHost[] = "127.0.0.1";
constexpr char kDefaultMysqlUser[] = "monitor";
constexpr char kDefaultMysqlPass[] = "monitor666";
constexpr char kDefaultMysqlDb[] = "monitor_db";

int main(int argc, char *argv[])
{
    std::string listen_address = kDefaultListenAddress;

    // 解析命令行参数
    if (argc > 1)
    {
        listen_address = argv[1];
    }

    std::cout << "Starting Monitor Client (Manager Mode)..." << std::endl;
    std::cout << "Listening on: " << listen_address << std::endl;

    // 创建 gRPC 服务
    monitor::GrpcServerImpl service;

    // 创建 HostManager 并设置回调
    monitor::HostManager mgr;
    service.SetDataReceivedCallback(
        [&mgr](const monitor::proto::MonitorInfo &info)
        {
            mgr.OnDataReceived(info);
        });

    // 启动 HostManager 后台处理
    mgr.Start();

    // 创建 QueryManager 并初始化
    monitor::QueryManager query_mgr;
#ifdef ENABLE_MYSQL
    if (query_mgr.Init(kDefaultMysqlHost, kDefaultMysqlUser, kDefaultMysqlPass,
                       kDefaultMysqlDb))
    {
        std::cout << "QueryManager initialized successfully" << std::endl;
    }
    else
    {
        std::cerr << "Warning: QueryManager initialization failed, "
                  << "query service will not be available" << std::endl;
    }
#endif

    // 创建查询服务
    monitor::QueryServiceImpl query_service(&query_mgr);

    // 启动 gRPC 服务器
    grpc::ServerBuilder builder;
    builder.AddListeningPort(listen_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    builder.RegisterService(&query_service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Monitor Client listening on " << listen_address << std::endl;
    std::cout << "Waiting for workers to push data..." << std::endl;
    std::cout << "Query service available for performance data queries" << std::endl;

    server->Wait();

    return 0;
}
