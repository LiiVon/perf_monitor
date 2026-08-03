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

#include "httplib.h"
#include "json.hpp"

using json = nlohmann::json;

constexpr char kDefaultListenAddress[] = "0.0.0.0:50051";
constexpr char kDefaultMysqlHost[] = "127.0.0.1";
constexpr char kDefaultMysqlUser[] = "monitor";
constexpr char kDefaultMysqlPass[] = "monitor666";
constexpr char kDefaultMysqlDb[] = "monitor_db";

int main(int argc, char *argv[])
{
    std::string listen_address = kDefaultListenAddress;

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

    // ===== 启动 HTTP 服务（在独立线程中） =====
    std::thread http_thread([&]()
                            {
        httplib::Server http_svr;

        // API: 获取所有主机的最新完整监控数据
        http_svr.Get("/api/latest", [&](const httplib::Request& req, httplib::Response& res) {
            auto all_hosts = mgr.GetAllHostScores();
            json response_json = json::array();

            for (const auto& [hostname, score_data] : all_hosts) {
                const auto& info = score_data.monitor_info;
                json item;

                // 1. 主机名 + 评分
                item["hostname"] = hostname;
                item["score"] = score_data.score;
                item["timestamp"] = std::chrono::system_clock::to_time_t(score_data.timestamp);

                // 2. 主机信息
                if (info.has_host_info()) {
                    item["host_info"]["hostname"] = info.host_info().hostname();
                    item["host_info"]["ip"] = info.host_info().ip_address();
                }

                // 3. CPU 负载
                if (info.has_cpu_load()) {
                    item["cpu_load"]["load_avg_1"] = info.cpu_load().load_avg_1();
                    item["cpu_load"]["load_avg_5"] = info.cpu_load().load_avg_5();
                    item["cpu_load"]["load_avg_15"] = info.cpu_load().load_avg_15();
                }

                // 4. CPU 统计（每个核心）
                json cpu_stats = json::array();
                for (int i = 0; i < info.cpu_stat_size(); ++i) {
                    const auto& cpu = info.cpu_stat(i);
                    cpu_stats.push_back({
                        {"cpu_name", cpu.cpu_name()},
                        {"cpu_percent", cpu.cpu_percent()},
                        {"usr_percent", cpu.usr_percent()},
                        {"system_percent", cpu.system_percent()},
                        {"nice_percent", cpu.nice_percent()},
                        {"idle_percent", cpu.idle_percent()},
                        {"io_wait_percent", cpu.io_wait_percent()},
                        {"irq_percent", cpu.irq_percent()},
                        {"soft_irq_percent", cpu.soft_irq_percent()}
                    });
                }
                item["cpu_stats"] = cpu_stats;

                // 5. 内存信息
                if (info.has_mem_info()) {
                    const auto& mem = info.mem_info();
                    item["mem_info"] = {
                        {"total", mem.total()},
                        {"free", mem.free()},
                        {"avail", mem.avail()},
                        {"used_percent", mem.used_percent()},
                        {"buffers", mem.buffers()},
                        {"cached", mem.cached()},
                        {"swap_cached", mem.swap_cached()},
                        {"active", mem.active()},
                        {"inactive", mem.inactive()},
                        {"active_anon", mem.active_anon()},
                        {"inactive_anon", mem.inactive_anon()},
                        {"active_file", mem.active_file()},
                        {"inactive_file", mem.inactive_file()},
                        {"dirty", mem.dirty()},
                        {"writeback", mem.writeback()},
                        {"anon_pages", mem.anon_pages()},
                        {"mapped", mem.mapped()},
                        {"kreclaimable", mem.kreclaimable()},
                        {"sreclaimable", mem.sreclaimable()},
                        {"sunreclaim", mem.sunreclaim()}
                    };
                }

                // 6. 网络信息
                json net_infos = json::array();
                for (int i = 0; i < info.net_info_size(); ++i) {
                    const auto& net = info.net_info(i);
                    net_infos.push_back({
                        {"name", net.name()},
                        {"send_rate", net.send_rate()},
                        {"rcv_rate", net.rcv_rate()},
                        {"send_packets_rate", net.send_packets_rate()},
                        {"rcv_packets_rate", net.rcv_packets_rate()},
                        {"err_in", net.err_in()},
                        {"err_out", net.err_out()},
                        {"drop_in", net.drop_in()},
                        {"drop_out", net.drop_out()}
                    });
                }
                item["net_infos"] = net_infos;

                // 7. 磁盘信息
                json disk_infos = json::array();
                for (int i = 0; i < info.disk_info_size(); ++i) {
                    const auto& disk = info.disk_info(i);
                    disk_infos.push_back({
                        {"name", disk.name()},
                        {"read_bytes_per_sec", disk.read_bytes_per_sec()},
                        {"write_bytes_per_sec", disk.write_bytes_per_sec()},
                        {"read_iops", disk.read_iops()},
                        {"write_iops", disk.write_iops()},
                        {"avg_read_latency_ms", disk.avg_read_latency_ms()},
                        {"avg_write_latency_ms", disk.avg_write_latency_ms()},
                        {"util_percent", disk.util_percent()},
                        {"reads", disk.reads()},
                        {"writes", disk.writes()},
                        {"io_in_progress", disk.io_in_progress()}
                    });
                }
                item["disk_infos"] = disk_infos;

                // 8. 软中断信息
                json soft_irqs = json::array();
                for (int i = 0; i < info.soft_irq_size(); ++i) {
                    const auto& irq = info.soft_irq(i);
                    soft_irqs.push_back({
                        {"cpu", irq.cpu()},
                        {"hi", irq.hi()},
                        {"timer", irq.timer()},
                        {"net_tx", irq.net_tx()},
                        {"net_rx", irq.net_rx()},
                        {"block", irq.block()},
                        {"irq_poll", irq.irq_poll()},
                        {"tasklet", irq.tasklet()},
                        {"sched", irq.sched()},
                        {"hrtimer", irq.hrtimer()},
                        {"rcu", irq.rcu()}
                    });
                }
                item["soft_irqs"] = soft_irqs;

                response_json.push_back(item);
            }

            res.set_content(response_json.dump(4), "application/json");
        });

        std::cout << "HTTP server listening on 0.0.0.0:50052" << std::endl;
        http_svr.listen("0.0.0.0", 50052); });

    http_thread.detach(); // 让 HTTP 服务在后台独立运行

    // ===== 阻塞等待 gRPC 服务器结束 =====
    server->Wait();

    return 0;
}