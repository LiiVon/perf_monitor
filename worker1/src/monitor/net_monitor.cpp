#include "monitor/net_monitor.h"
#include "monitor_info.grpc.pb.h"
#include "monitor_info.pb.h"
#include "utils/read_file.h"
#include <chrono>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace monitor
{
    static const char *kProcNetDevPath = "/proc/net/dev";
    static std::vector<NetStat> get_net_stats_from_proc()
    {
        std::vector<NetStat> net_stats;
        ReadFile rf(kProcNetDevPath);
        std::vector<std::string> args;
        // 跳过前两行标题（"Inter-| Receive ..." 和 " face |bytes ..."）
        if (!rf.ReadLine(&args))
            return net_stats;
        args.clear();
        if (!rf.ReadLine(&args))
            return net_stats;
        args.clear();

        // 读取每一行网络接口信息
        while (rf.ReadLine(&args))
        {
            if (args.size() < 17)
            {
                continue; // 如果行中没有足够的字段，跳过该行
            }

            std::string iface = args[0];
            // 去掉冒号
            if (iface.back() == ':')
            {
                iface.pop_back();
            }
            // 跳过回环接口
            if (iface == "lo")
            {
                args.clear();
                continue;
            }

            NetStat net_stat;
            net_stat.name = iface;
            net_stat.rcv_bytes = std::stoull(args[1]);
            net_stat.rcv_packets = std::stoull(args[2]);
            net_stat.snd_bytes = std::stoull(args[9]);
            net_stat.snd_packets = std::stoull(args[10]);
            net_stat.err_in = std::stoull(args[3]);
            net_stat.err_out = std::stoull(args[11]);
            net_stat.drop_in = std::stoull(args[4]);
            net_stat.drop_out = std::stoull(args[12]);

            net_stats.push_back(net_stat);
            args.clear();
        }
        return net_stats;
    }

    void NetMonitor::UpdateOnce(monitor::proto::MonitorInfo *monitor_info)
    {
        auto net_stats = get_net_stats_from_proc();
        auto now = std::chrono::steady_clock::now();

        for (const auto &net_stat : net_stats)
        {
            auto it = last_net_info_.find(net_stat.name);
            double rcv_rate = 0, rcv_packets_rate = 0, send_rate = 0,
                   send_packets_rate = 0;
            if (it != last_net_info_.end())
            {
                const auto &last_info = it->second;
                double time_diff = std::chrono::duration<double>(now - last_info.timepoint).count();
                if (time_diff > 0)
                {
                    // kbs
                    rcv_rate = (net_stat.rcv_bytes - last_info.rcv_bytes) / 1024.0 / time_diff;
                    send_rate = (net_stat.snd_bytes - last_info.snd_bytes) / 1024.0 / time_diff;

                    rcv_packets_rate = (net_stat.rcv_packets - last_info.rcv_packets) / time_diff;
                    send_packets_rate = (net_stat.snd_packets - last_info.snd_packets) / time_diff;
                }
            }
            // 填充 protobuf
            auto *net_info = monitor_info->add_net_info();
            net_info->set_name(net_stat.name);
            net_info->set_rcv_rate(rcv_rate);
            net_info->set_rcv_packets_rate(rcv_packets_rate);
            net_info->set_send_rate(send_rate);
            net_info->set_send_packets_rate(send_packets_rate);
            // 错误和丢弃统计
            net_info->set_err_in(net_stat.err_in);
            net_info->set_err_out(net_stat.err_out);
            net_info->set_drop_in(net_stat.drop_in);
            net_info->set_drop_out(net_stat.drop_out);

            // 更新最后一次的网络信息
            last_net_info_[net_stat.name] = NetInfo{
                net_stat.name,
                net_stat.rcv_bytes,
                net_stat.rcv_packets,
                net_stat.snd_bytes,
                net_stat.snd_packets,
                net_stat.err_in,
                net_stat.err_out,
                net_stat.drop_in,
                net_stat.drop_out,
                now};
        }
    }

    void NetMonitor::Stop()
    {
        // 目前没有需要清理的资源
    }
}
