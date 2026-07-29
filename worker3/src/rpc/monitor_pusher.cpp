#include "rpc/monitor_pusher.h"
#include <chrono>
#include <iostream>

namespace monitor
{
    MonitorPusher::MonitorPusher(const std::string &manager_addr, int interval_seconds)
        : manager_addr_(manager_addr), interval_seconds_(interval_seconds),
          running_(false)
    {
        // 创建 MetricCollector
        collector_ = std::make_unique<MetricCollector>();

        // 创建 gRPC stub
        auto channel = grpc::CreateChannel(manager_addr_, grpc::InsecureChannelCredentials());
        stub_ = monitor::proto::GrpcManager::NewStub(channel);
    }

    MonitorPusher::~MonitorPusher()
    {
        Stop();
    }

    void MonitorPusher::Start()
    {
        if (running_)
            return;
        running_ = true;
        // 启动推送线程,即调用 MonitorPusher::PushLoop() 方法
        thread_ = std::make_unique<std::thread>(&MonitorPusher::PushLoop, this);
        std::cout << "MonitorPusher started, pushing to " << manager_addr_
                  << "every " << interval_seconds_ << " seconds." << std::endl;
    }

    void MonitorPusher::Stop()
    {
        if (!running_)
            return;
        running_ = false;
        if (thread_ && thread_->joinable())
        {
            thread_->join();
        }
        std::cout << "MonitorPusher stopped." << std::endl;
    }

    void MonitorPusher::PushLoop()
    {
        while (running_)
        {
            if (!PushOnce())
            {
                std::cerr << "Failed to push monitor info to manager:" << manager_addr_ << std::endl;
            }

            for (int i = 0; i < interval_seconds_ && running_; ++i)
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }

    // 推送一次监控信息
    bool MonitorPusher::PushOnce()
    {
        // 采集监控信息
        monitor::proto::MonitorInfo info;
        collector_->CollectAll(&info);

        // 打印采集到的监控信息
        std::cout << "\n================== Collected Metrics =================="
                  << std::endl;

        // 主机信息
        if (info.has_host_info())
        {
            std::cout << "[Host] Hostname: " << info.host_info().hostname()
                      << ", IP: " << info.host_info().ip_address() << std::endl;
        }

        // CPU 统计信息
        std::cout << "\n--- CPU Statistics ---" << std::endl;
        for (int i = 0; i < info.cpu_stat_size(); ++i)
        {
            const auto &cpu = info.cpu_stat(i);
            std::cout << "[" << cpu.cpu_name() << "] "
                      << "Total: " << cpu.cpu_percent() << "%, "
                      << "User: " << cpu.usr_percent() << "%, "
                      << "System: " << cpu.system_percent() << "%, "
                      << "Nice: " << cpu.nice_percent() << "%, "
                      << "Idle: " << cpu.idle_percent() << "%, "
                      << "IOWait: " << cpu.io_wait_percent() << "%, "
                      << "IRQ: " << cpu.irq_percent() << "%, "
                      << "SoftIRQ: " << cpu.soft_irq_percent() << "%" << std::endl;
        }

        // CPU 负载信息
        if (info.has_cpu_load())
        {
            std::cout << "\n--- CPU Load ---" << std::endl;
            std::cout << "[Load] 1min: " << info.cpu_load().load_avg_1()
                      << ", 5min: " << info.cpu_load().load_avg_5()
                      << ", 15min: " << info.cpu_load().load_avg_15() << std::endl;
        }

        // 软中断信息 - 所有 CPU 核心
        if (info.soft_irq_size() > 0)
        {
            std::cout << "\n--- SoftIRQ Info ---" << std::endl;
            for (int i = 0; i < info.soft_irq_size(); ++i)
            {
                const auto &sirq = info.soft_irq(i);
                std::cout << "[" << sirq.cpu() << "] "
                          << "HI: " << sirq.hi() << ", "
                          << "TIMER: " << sirq.timer() << ", "
                          << "NET_TX: " << sirq.net_tx() << ", "
                          << "NET_RX: " << sirq.net_rx() << ", "
                          << "BLOCK: " << sirq.block() << ", "
                          << "IRQ_POLL: " << sirq.irq_poll() << ", "
                          << "TASKLET: " << sirq.tasklet() << ", "
                          << "SCHED: " << sirq.sched() << ", "
                          << "HRTIMER: " << sirq.hrtimer() << ", "
                          << "RCU: " << sirq.rcu() << std::endl;
            }
        }

        // 内存信息 - 所有字段
        if (info.has_mem_info())
        {
            const auto &mem = info.mem_info();
            std::cout << "\n--- Memory Info ---" << std::endl;
            std::cout << "[Memory] Used: " << mem.used_percent() << "%" << std::endl;
            std::cout << "  Total: " << mem.total() << " MB, "
                      << "Free: " << mem.free() << " MB, "
                      << "Avail: " << mem.avail() << " MB" << std::endl;
            std::cout << "  Buffers: " << mem.buffers() << " MB, "
                      << "Cached: " << mem.cached() << " MB, "
                      << "SwapCached: " << mem.swap_cached() << " MB" << std::endl;
            std::cout << "  Active: " << mem.active() << " MB, "
                      << "Inactive: " << mem.inactive() << " MB" << std::endl;
            std::cout << "  ActiveAnon: " << mem.active_anon() << " MB, "
                      << "InactiveAnon: " << mem.inactive_anon() << " MB" << std::endl;
            std::cout << "  ActiveFile: " << mem.active_file() << " MB, "
                      << "InactiveFile: " << mem.inactive_file() << " MB" << std::endl;
            std::cout << "  Dirty: " << mem.dirty() << " MB, "
                      << "Writeback: " << mem.writeback() << " MB" << std::endl;
            std::cout << "  AnonPages: " << mem.anon_pages() << " MB, "
                      << "Mapped: " << mem.mapped() << " MB" << std::endl;
            std::cout << "  KReclaimable: " << mem.kreclaimable() << " MB, "
                      << "SReclaimable: " << mem.sreclaimable() << " MB, "
                      << "SUnreclaim: " << mem.sunreclaim() << " MB" << std::endl;
        }

        // 网络信息 - 所有网卡所有字段
        if (info.net_info_size() > 0)
        {
            std::cout << "\n--- Network Info ---" << std::endl;
            for (int i = 0; i < info.net_info_size(); ++i)
            {
                const auto &net = info.net_info(i);
                std::cout << "[" << net.name() << "]" << std::endl;
                std::cout << "  Recv: " << net.rcv_rate() << " B/s ("
                          << net.rcv_packets_rate() << " pkt/s)" << std::endl;
                std::cout << "  Send: " << net.send_rate() << " B/s ("
                          << net.send_packets_rate() << " pkt/s)" << std::endl;
                std::cout << "  Errors(in/out): " << net.err_in() << "/" << net.err_out()
                          << ", Drops(in/out): " << net.drop_in() << "/" << net.drop_out()
                          << std::endl;
            }
        }

        // 磁盘信息 - 所有磁盘所有字段
        if (info.disk_info_size() > 0)
        {
            std::cout << "\n--- Disk Info ---" << std::endl;
            for (int i = 0; i < info.disk_info_size(); ++i)
            {
                const auto &disk = info.disk_info(i);
                std::cout << "[" << disk.name() << "]" << std::endl;
                std::cout << "  Read: " << disk.read_bytes_per_sec() / 1024.0 << " KB/s, "
                          << "IOPS: " << disk.read_iops() << ", "
                          << "Latency: " << disk.avg_read_latency_ms() << " ms"
                          << std::endl;
                std::cout << "  Write: " << disk.write_bytes_per_sec() / 1024.0
                          << " KB/s, "
                          << "IOPS: " << disk.write_iops() << ", "
                          << "Latency: " << disk.avg_write_latency_ms() << " ms"
                          << std::endl;
                std::cout << "  Util: " << disk.util_percent() << "%, "
                          << "IO_InProgress: " << disk.io_in_progress() << std::endl;
                std::cout << "  Reads: " << disk.reads() << ", "
                          << "Writes: " << disk.writes() << ", "
                          << "SectorsRead: " << disk.sectors_read() << ", "
                          << "SectorsWritten: " << disk.sectors_written() << std::endl;
            }
        }

        std::cout << "========================================================\n"
                  << std::endl;

        // 发送 gRPC 请求
        grpc::ClientContext context;
        google::protobuf::Empty response;
        grpc ::Status status = stub_->SetMonitorInfo(&context, info, &response);

        if (status.ok())
        {
            std::cout << ">>> [" << std::time(nullptr) << "] "
                      << "Successfully pushed monitor info to manager: "
                      << manager_addr_ << " <<<" << std::endl;
            return true;
        }
        else
        {
            std::cerr << ">>> [" << std::time(nullptr) << "] "
                      << "Failed to push monitor info to manager: "
                      << manager_addr_ << ", error: " << status.error_message()
                      << " (code " << status.error_code() << ") <<<" << std::endl;
            return false;
        }
    }
}
