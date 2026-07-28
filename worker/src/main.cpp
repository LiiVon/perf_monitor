#include "monitor/metric_collector.h"
#include "monitor_info.grpc.pb.h"
#include "monitor_info.pb.h"
#include <grpcpp/grpcpp.h>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>

// int main()
// {
//     monitor::ReadFile rf("/proc/stat");
//     std::vector<std::string> fields;
//     if (rf.ReadLine(&fields))
//     {
//         for (const auto &s : fields)
//         {
//             std::cout << s << " ";
//         }
//         std::cout << std::endl;
//     }
//     return 0;
// }

// 全局标志，用于优雅退出
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
    // 1. 解析命令行参数：Manager 服务地址
    std::string manager_addr = "localhost:50051"; // 默认
    if (argc >= 2)
    {
        manager_addr = argv[1];
    }
    std::cout << "Connecting to Manager at: " << manager_addr << std::endl;

    // 2. 设置信号处理（Ctrl+C 优雅退出）
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    // 3. 创建 gRPC channel 和 stub
    auto channel = grpc::CreateChannel(manager_addr, grpc::InsecureChannelCredentials());
    auto stub = monitor::proto::GrpcManager::NewStub(channel);

    // 4. 创建监控采集器
    monitor::MetricCollector collector;

    // 5. 主循环：采集并推送
    const int collect_interval_seconds = 3; // 采集间隔（秒）
    while (running)
    {
        // 5.1 采集数据
        monitor::proto::MonitorInfo info;
        collector.CollectAll(&info);

        // 5.2 构造 gRPC 请求
        grpc::ClientContext context;
        google::protobuf::Empty response;
        auto status = stub->SetMonitorInfo(&context, info, &response);

        // 5.3 处理结果
        if (status.ok())
        {
            std::cout << "[" << std::time(nullptr) << "] "
                      << "Data pushed successfully." << std::endl;
        }
        else
        {
            std::cerr << "Failed to push data: " << status.error_message()
                      << " (code " << status.error_code() << ")" << std::endl;
        }

        // 5.4 等待下一个采集周期
        for (int i = 0; i < collect_interval_seconds && running; ++i)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    std::cout << "Worker stopped gracefully." << std::endl;
    return 0;
}