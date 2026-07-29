#include "monitor/metric_collector.h"
#include "monitor_info.grpc.pb.h"
#include "monitor_info.pb.h"
#include "rpc/monitor_pusher.h"
#include <grpcpp/grpcpp.h>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>

/*
一 main
int main()
{
    monitor::ReadFile rf("/proc/stat");
    std::vector<std::string> fields;
    if (rf.ReadLine(&fields))
    {
        for (const auto &s : fields)
        {
            std::cout << s << " ";
        }
        std::cout << std::endl;
    }
    return 0;
}
*/



// //二 main
// // 全局标志，用于优雅退出
// // static bool running = true;
// void SignalHandler(int signal)
// {
//     if (signal == SIGINT || signal == SIGTERM)
//     {
//         std::cout << "Received exit signal, stopping..." << std::endl;
//         running = false;
//     }
// }
// int main(int argc, char *argv[])
// {
//     // 解析命令行参数：Manager 服务地址
//     std::string manager_addr = "localhost:50051"; // 默认
//     if (argc >= 2)
//     {
//         manager_addr = argv[1];
//     }
//     std::cout << "Connecting to Manager at: " << manager_addr << std::endl;
//     // 设置信号处理（Ctrl+C 优雅退出）
//     std::signal(SIGINT, SignalHandler);
//     std::signal(SIGTERM, SignalHandler);
//     // manager_addr：就是你要拨打的“电话号码”（例如 localhost:50051 或 192.168.1.100:50051）。
//     // CreateChannel：这是 gRPC 的“总机”。它会在底层创建一个 HTTP/2 连接池（实际 TCP 连接可能延迟到首次调用时才建立）。它负责管理网络连接、重试和负载均衡。
//     // InsecureChannelCredentials：表示当前是“明文通话”（无加密）。在生产环境，这里通常会换成 SSL/TLS 证书（SslCredentials）。
//     auto channel = grpc::CreateChannel(manager_addr, grpc::InsecureChannelCredentials());
//     /*
//     Stub（存根）：它就像一个“本地翻译官”。你在代码里调用 stub->SetMonitorInfo(...)，它立刻把这个调用翻译成网络数据包（Protobuf 二进制），通过刚才建立好的 Channel 发送出去。
//     它屏蔽了底层的网络通信细节，让你像调用本地函数一样调用远程 Manager 的服务。
//     */
// auto stub = monitor::proto::GrpcManager::NewStub(channel);
// // 创建监控采集器
// monitor::MetricCollector collector;
// // 主循环：采集并推送
// const int collect_interval_seconds = 10; // 采集间隔（秒）
// while (running)
// {
//     // 采集数据
//     monitor::proto::MonitorInfo info;
//     collector.CollectAll(&info);
//     /*
//     ClientContext：这是这次通话的“辅助信息包”。你可以往里面塞东西，比如：
//     设置超时时间（context.set_deadline(...)）。
//     添加认证 Token（context.AddMetadata("authorization", "Bearer xxx")）。
//     但你现在没设置，表示使用默认配置。
//     Empty response：根据 proto 定义，SetMonitorInfo 只接收数据，不返回任何业务数据（只返回一个空包）。这个变量就是用来接收那个“空包”的容器。
//     */
//     grpc::ClientContext context;
//     google::protobuf::Empty response;
//     /*
//     同步（阻塞）地执行了远程调用：
//     序列化：自动将 info 对象（MonitorInfo）编码成二进制格式（Protobuf 序列化）。
//     发送：通过 HTTP/2 流将数据发送给 Manager 的 50051 端口。
//     等待：当前线程在此阻塞，等待 Manager 处理完毕并返回响应（或超时/出错）。
//     接收：Manager 返回的 Empty 数据被反序列化填入 response。
//     */
//     auto status = stub->SetMonitorInfo(&context, info, &response);
//     // 处理结果
//     if (status.ok())
//     {
//         std::cout << "[" << std::time(nullptr) << "] "
//                   << "Data pushed successfully." << std::endl;
//     }
//     else
//     {
//         std::cerr << "Failed to push data: " << status.error_message()
//                   << " (code " << status.error_code() << ")" << std::endl;
//     }
//     // 等待下一个采集周期
//     for (int i = 0; i < collect_interval_seconds && running; ++i)
//     {
//         std::this_thread::sleep_for(std::chrono::seconds(1));
//     }
// }
// std::cout << "Worker stopped gracefully." << std::endl;
// return 0;
// }


// 三 main
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
    std::string manager_addr = "localhost:50051"; // 默认
    if(argc >= 2)
    {
        manager_addr = argv[1];
    }
    // 设置信号处理函数
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    // 创建 MonitorPusher 实例 默认刷新间隔为10秒,这里修改5s
    monitor::MonitorPusher pusher(manager_addr,5);
    pusher.Start();

    // 主循环
    while (running)
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    // 停止推送
    pusher.Stop();
    std::cout << "Worker stopped gracefully." << std::endl;
    return 0;
}