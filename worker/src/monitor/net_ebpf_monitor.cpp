#include "monitor/net_ebpf_monitor.h"
#include "monitor_info.pb.h"
#include "../ebpf/.output/net_stats.skel.h"

#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <net/if.h>
#include <linux/if_link.h>
#include <linux/pkt_sched.h>
#include <dirent.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <chrono>

struct net_stats
{
    uint64_t rcv_bytes;
    uint64_t rcv_packets;
    uint64_t snd_bytes;
    uint64_t snd_packets;
};

namespace monitor
{

    // ==================== 辅助函数 ====================

    static std::vector<uint32_t> GetAllIfIndexes()
    {
        std::vector<uint32_t> indexes;
        DIR *dir = opendir("/sys/class/net");
        if (!dir)
            return indexes;

        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            if (entry->d_name[0] == '.')
                continue;

            // 跳过 lo 和虚拟接口
            if (strcmp(entry->d_name, "lo") == 0)
                continue;
            if (strncmp(entry->d_name, "docker", 6) == 0)
                continue;
            if (strncmp(entry->d_name, "veth", 4) == 0)
                continue;

            unsigned int ifindex = if_nametoindex(entry->d_name);
            if (ifindex > 0)
            {
                indexes.push_back(ifindex);
            }
        }
        closedir(dir);
        return indexes;
    }

    // ==================== 构造/析构 ====================

    NetEbpfMonitor::NetEbpfMonitor()
        : skel_(nullptr), map_fd_(-1), loaded_(false)
    {
        loaded_ = InitEbpf();
        if (!loaded_)
        {
            std::cerr << "NetEbpfMonitor: Failed to load eBPF program, "
                      << "falling back to /proc/net/dev" << std::endl;
        }
    }

    NetEbpfMonitor::~NetEbpfMonitor()
    {
        CleanupEbpf();
    }

    // ==================== 初始化和清理 ====================

    bool NetEbpfMonitor::InitEbpf()
    {
        // 1. 打开 skeleton
        skel_ = net_stats_bpf__open();
        if (!skel_)
        {
            std::cerr << "Failed to open BPF skeleton" << std::endl;
            return false;
        }

        // 2. 加载到内核
        int err = net_stats_bpf__load(skel_);
        if (err)
        {
            std::cerr << "Failed to load BPF program: " << strerror(-err) << std::endl;
            net_stats_bpf__destroy(skel_);
            skel_ = nullptr;
            return false;
        }

        // 3. 获取 Map fd
        map_fd_ = bpf_map__fd(skel_->maps.net_stats_map);
        if (map_fd_ < 0)
        {
            std::cerr << "Failed to get map fd" << std::endl;
            net_stats_bpf__destroy(skel_);
            skel_ = nullptr;
            return false;
        }

        // 4. 获取程序 fd
        int ingress_fd = bpf_program__fd(skel_->progs.tc_ingress);
        int egress_fd = bpf_program__fd(skel_->progs.tc_egress);
        if (ingress_fd < 0 || egress_fd < 0)
        {
            std::cerr << "Failed to get program fds" << std::endl;
            net_stats_bpf__destroy(skel_);
            skel_ = nullptr;
            return false;
        }

        // 5. 遍历网卡，附加 TC hook
        auto ifindexes = GetAllIfIndexes();
        if (ifindexes.empty())
        {
            std::cerr << "No network interfaces found" << std::endl;
            net_stats_bpf__destroy(skel_);
            skel_ = nullptr;
            return false;
        }

        for (uint32_t ifindex : ifindexes)
        {
            char ifname[IF_NAMESIZE];
            if (if_indextoname(ifindex, ifname) == nullptr)
                continue;

            // 缓存网卡名
            ifindex_to_name_[ifindex] = ifname;

            // 创建 clsact qdisc（使用 libbpf API）
            LIBBPF_OPTS(bpf_tc_hook, hook,
                        .ifindex = static_cast<int>(ifindex),
                        .attach_point = BPF_TC_INGRESS);

            err = bpf_tc_hook_create(&hook);
            if (err && err != -EEXIST)
            {
                std::cerr << "Failed to create TC hook for " << ifname
                          << ": " << strerror(-err) << std::endl;
                continue;
            }

            // 附加 ingress
            hook.attach_point = BPF_TC_INGRESS;
            LIBBPF_OPTS(bpf_tc_opts, opts_in, .prog_fd = ingress_fd);
            err = bpf_tc_attach(&hook, &opts_in);
            if (err)
            {
                std::cerr << "Failed to attach ingress for " << ifname
                          << ": " << strerror(-err) << std::endl;
            }
            else
            {
                attached_ifindexes_.push_back(ifindex);
                std::cout << "Attached TC ingress to " << ifname << std::endl;
            }

            // 附加 egress
            hook.attach_point = BPF_TC_EGRESS;
            LIBBPF_OPTS(bpf_tc_opts, opts_eg, .prog_fd = egress_fd);
            err = bpf_tc_attach(&hook, &opts_eg);
            if (err)
            {
                std::cerr << "Failed to attach egress for " << ifname
                          << ": " << strerror(-err) << std::endl;
            }
            else
            {
                std::cout << "Attached TC egress to " << ifname << std::endl;
            }
        }

        if (attached_ifindexes_.empty())
        {
            std::cerr << "No interfaces attached" << std::endl;
            net_stats_bpf__destroy(skel_);
            skel_ = nullptr;
            return false;
        }

        last_update_time_ = std::chrono::steady_clock::now();
        std::cout << "NetEbpfMonitor: eBPF TC hook loaded successfully" << std::endl;
        return true;
    }

    void NetEbpfMonitor::CleanupEbpf()
    {
        // 分离 TC hook
        for (uint32_t ifindex : attached_ifindexes_)
        {
            LIBBPF_OPTS(bpf_tc_hook, hook,
                        .ifindex = static_cast<int>(ifindex),
                        .attach_point = BPF_TC_INGRESS);

            LIBBPF_OPTS(bpf_tc_opts, opts);
            bpf_tc_detach(&hook, &opts);

            hook.attach_point = BPF_TC_EGRESS;
            bpf_tc_detach(&hook, &opts);
        }
        attached_ifindexes_.clear();

        if (skel_)
        {
            net_stats_bpf__destroy(skel_);
            skel_ = nullptr;
        }
        map_fd_ = -1;
        loaded_ = false;
    }

    bool NetEbpfMonitor::IsLoaded()
    {
        return loaded_;
    }

    // ==================== 网卡名获取 ====================

    std::string NetEbpfMonitor::GetIfName(uint32_t ifindex)
    {
        auto it = ifindex_to_name_.find(ifindex);
        if (it != ifindex_to_name_.end())
        {
            return it->second;
        }

        char ifname[IF_NAMESIZE];
        if (if_indextoname(ifindex, ifname) != nullptr)
        {
            std::string name(ifname);
            ifindex_to_name_[ifindex] = name;
            return name;
        }

        return "";
    }

    // ==================== 核心采集函数 ====================

    void NetEbpfMonitor::UpdateOnce(monitor::proto::MonitorInfo *monitor_info)
    {
        if (!monitor_info || !loaded_ || map_fd_ < 0)
        {
            return;
        }

        auto now = std::chrono::steady_clock::now();
        double time_diff = std::chrono::duration<double>(now - last_update_time_).count();
        if (time_diff <= 0)
        {
            time_diff = 0.001; // 防止除零
        }

        // 遍历 BPF Map
        uint32_t key = 0, next_key;
        struct net_stats stats; // 由手动定义提供

        while (bpf_map_get_next_key(map_fd_, &key, &next_key) == 0)
        {
            if (bpf_map_lookup_elem(map_fd_, &next_key, &stats) != 0)
            {
                key = next_key;
                continue;
            }

            // 获取网卡名
            auto it_name = ifindex_to_name_.find(next_key);
            if (it_name == ifindex_to_name_.end())
            {
                key = next_key;
                continue;
            }
            std::string ifname = it_name->second;

            // 查找上次缓存
            auto cache_it = cache_.find(next_key);
            auto *net_info = monitor_info->add_net_info();
            net_info->set_name(ifname);

            if (cache_it != cache_.end())
            {
                const auto &old = cache_it->second;

                // 计算差值（处理可能的计数器回绕）
                int64_t rcv_bytes_diff = static_cast<int64_t>(stats.rcv_bytes - old.rcv_bytes);
                int64_t snd_bytes_diff = static_cast<int64_t>(stats.snd_bytes - old.snd_bytes);
                int64_t rcv_packets_diff = static_cast<int64_t>(stats.rcv_packets - old.rcv_packets);
                int64_t snd_packets_diff = static_cast<int64_t>(stats.snd_packets - old.snd_packets);

                // 如果差值小于0，说明计数器回绕了，直接使用当前值
                if (rcv_bytes_diff < 0)
                    rcv_bytes_diff = static_cast<int64_t>(stats.rcv_bytes);
                if (snd_bytes_diff < 0)
                    snd_bytes_diff = static_cast<int64_t>(stats.snd_bytes);
                if (rcv_packets_diff < 0)
                    rcv_packets_diff = static_cast<int64_t>(stats.rcv_packets);
                if (snd_packets_diff < 0)
                    snd_packets_diff = static_cast<int64_t>(stats.snd_packets);

                // 速率 = 差值 / 时间间隔，单位 B/s
                net_info->set_rcv_rate(static_cast<double>(rcv_bytes_diff) / time_diff);
                net_info->set_send_rate(static_cast<double>(snd_bytes_diff) / time_diff);
                net_info->set_rcv_packets_rate(static_cast<double>(rcv_packets_diff) / time_diff);
                net_info->set_send_packets_rate(static_cast<double>(snd_packets_diff) / time_diff);
            }
            else
            {
                // 首次采集，速率为 0
                net_info->set_rcv_rate(0);
                net_info->set_send_rate(0);
                net_info->set_rcv_packets_rate(0);
                net_info->set_send_packets_rate(0);
            }

            // 更新缓存（字段顺序与 BPF Map 中的 net_stats 一致！）
            NetStatCache cached;
            cached.rcv_bytes = stats.rcv_bytes;
            cached.rcv_packets = stats.rcv_packets;
            cached.snd_bytes = stats.snd_bytes;
            cached.snd_packets = stats.snd_packets;
            cached.timestamp = now;
            cache_[next_key] = cached;

            key = next_key;
        }

        last_update_time_ = now;
    }

    void NetEbpfMonitor::Stop()
    {
        CleanupEbpf();
    }

} // namespace monitor