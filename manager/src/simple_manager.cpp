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

class SimpleManagerImpl final : public GrpcManager::Service
{
public:
    Status SetMonitorInfo(ServerContext *context,
                          const MonitorInfo *request,
                          Empty *response) override
    {
        std::cout << "Received data from: " << request->name() << std::endl;
        // 可选：打印部分数据验证
        if (request->has_cpu_load())
        {
            std::cout << "  CPU Load: " << request->cpu_load().load_avg_1() << std::endl;
        }
        return Status::OK;
    }
    Status GetMonitorInfo(ServerContext *context,
                          const Empty *request,
                          MonitorInfo *response) override
    {
        return Status::OK;
    }
};

int main()
{
    std::string server_addr = "0.0.0.0:50051";
    SimpleManagerImpl service;
    ServerBuilder builder;
    builder.AddListeningPort(server_addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    if (!server)
    {
        std::cerr << "Failed to start server." << std::endl;
        return 1;
    }
    std::cout << "Manager listening on " << server_addr << std::endl;
    server->Wait();
    return 0;
}