#include "host_manager.h"

#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

#ifdef ENABLE_MYSQL
#include <mysql/mysql.h>
#endif

namespace monitor
{

    // ==================== 内部辅助结构 ====================

    // 用于变化率计算的性能采样数据（内部使用）
    struct PerfSample
    {
        float cpu_percent = 0, usr_percent = 0, system_percent = 0;
        float nice_percent = 0, idle_percent = 0, io_wait_percent = 0;
        float irq_percent = 0, soft_irq_percent = 0;
        float steal_percent = 0, guest_percent = 0, guest_nice_percent = 0;
        float load_avg_1 = 0, load_avg_5 = 0, load_avg_15 = 0;
        float mem_used_percent = 0, mem_total = 0, mem_free = 0, mem_avail = 0;
        float net_in_rate = 0, net_out_rate = 0;
        float score = 0;
    };

#ifdef ENABLE_MYSQL
    // 用于详细表变化率计算的历史数据（保留原结构）
    struct NetDetailSample
    {
        float rcv_bytes_rate = 0;
        float rcv_packets_rate = 0;
        float snd_bytes_rate = 0;
        float snd_packets_rate = 0;
        uint64_t err_in = 0;
        uint64_t err_out = 0;
        uint64_t drop_in = 0;
        uint64_t drop_out = 0;
    };

    struct SoftIrqSample
    {
        float hi = 0, timer = 0, net_tx = 0, net_rx = 0, block = 0;
        float irq_poll = 0, tasklet = 0, sched = 0, hrtimer = 0, rcu = 0;
    };

    struct MemDetailSample
    {
        float total = 0, free = 0, avail = 0, buffers = 0, cached = 0;
        float swap_cached = 0, active = 0, inactive = 0;
        float active_anon = 0, inactive_anon = 0, active_file = 0, inactive_file = 0;
        float dirty = 0, writeback = 0, anon_pages = 0, mapped = 0;
        float kreclaimable = 0, sreclaimable = 0, sunreclaim = 0;
    };

    struct DiskDetailSample
    {
        float read_bytes_per_sec = 0;
        float write_bytes_per_sec = 0;
        float read_iops = 0;
        float write_iops = 0;
        float avg_read_latency_ms = 0;
        float avg_write_latency_ms = 0;
        float util_percent = 0;
    };

    // 历史数据存储（静态全局，保留原逻辑）
    static std::map<std::string, std::map<std::string, NetDetailSample>> last_net_samples;
    static std::map<std::string, std::map<std::string, SoftIrqSample>> last_softirq_samples;
    static std::map<std::string, MemDetailSample> last_mem_samples;
    static std::map<std::string, std::map<std::string, DiskDetailSample>> last_disk_samples;

    // MySQL 连接信息
    namespace
    {
        const char *MYSQL_HOST = "127.0.0.1";
        const char *MYSQL_USER = "monitor";
        const char *MYSQL_PASS = "monitor666";
        const char *MYSQL_DB = "monitor_db";
    }
#endif

    // 用于变化率计算的上一轮采样缓存
    static std::map<std::string, PerfSample> last_perf_samples;

    // ==================== 辅助函数实现 ====================

    std::string HostManager::ExtractHostName(const monitor::proto::MonitorInfo &info)
    {
        // 【优先】使用顶层 name 字段（由 MetricCollector 设置）
        if (!info.name().empty())
        {
            return info.name();
        }

        // 【回退】使用 host_info
        std::string host_name;
        if (info.has_host_info())
        {
            const auto &host_info = info.host_info();
            std::string hostname = host_info.hostname();
            std::string ip = host_info.ip_address();
            if (!hostname.empty() && !ip.empty())
                host_name = hostname + "_" + ip;
            else if (!hostname.empty())
                host_name = hostname;
            else if (!ip.empty())
                host_name = ip;
        }
        if (host_name.empty())
            host_name = "unknown";
        return host_name;
    }

    void HostManager::ExtractPerfData(const monitor::proto::MonitorInfo &info,
                                      double net_in_rate,
                                      double net_out_rate,
                                      double score,
                                      PerfSample &curr)
    {
        if (info.cpu_stat_size() > 0)
        {
            const auto &cpu = info.cpu_stat(0);
            curr.cpu_percent = cpu.cpu_percent();
            curr.usr_percent = cpu.usr_percent();
            curr.system_percent = cpu.system_percent();
            curr.nice_percent = cpu.nice_percent();
            curr.idle_percent = cpu.idle_percent();
            curr.io_wait_percent = cpu.io_wait_percent();
            curr.irq_percent = cpu.irq_percent();
            curr.soft_irq_percent = cpu.soft_irq_percent();
        }
        if (info.has_cpu_load())
        {
            curr.load_avg_1 = info.cpu_load().load_avg_1();
            curr.load_avg_5 = info.cpu_load().load_avg_5();
            curr.load_avg_15 = info.cpu_load().load_avg_15();
        }
        if (info.has_mem_info())
        {
            const auto &mem = info.mem_info();
            curr.mem_used_percent = mem.used_percent();
            curr.mem_total = mem.total();
            curr.mem_free = mem.free();
            curr.mem_avail = mem.avail();
        }
        curr.net_in_rate = net_in_rate;
        curr.net_out_rate = net_out_rate;
        curr.score = score;
    }

    RateInfo HostManager::ComputeRates(const PerfSample &curr, const PerfSample &last)
    {
        auto rate = [](float now_val, float last_val) -> float
        {
            if (last_val == 0)
                return 0;
            return (now_val - last_val) / last_val;
        };

        RateInfo r;
        r.cpu_percent = rate(curr.cpu_percent, last.cpu_percent);
        r.usr_percent = rate(curr.usr_percent, last.usr_percent);
        r.system_percent = rate(curr.system_percent, last.system_percent);
        r.nice_percent = rate(curr.nice_percent, last.nice_percent);
        r.idle_percent = rate(curr.idle_percent, last.idle_percent);
        r.io_wait_percent = rate(curr.io_wait_percent, last.io_wait_percent);
        r.irq_percent = rate(curr.irq_percent, last.irq_percent);
        r.soft_irq_percent = rate(curr.soft_irq_percent, last.soft_irq_percent);
        r.load_avg_1 = rate(curr.load_avg_1, last.load_avg_1);
        r.load_avg_5 = rate(curr.load_avg_5, last.load_avg_5);
        r.load_avg_15 = rate(curr.load_avg_15, last.load_avg_15);
        r.mem_used_percent = rate(curr.mem_used_percent, last.mem_used_percent);
        r.mem_total = rate(curr.mem_total, last.mem_total);
        r.mem_free = rate(curr.mem_free, last.mem_free);
        r.mem_avail = rate(curr.mem_avail, last.mem_avail);
        r.net_in_rate = rate(curr.net_in_rate, last.net_in_rate);
        r.net_out_rate = rate(curr.net_out_rate, last.net_out_rate);
        // 未使用的字段保留为0
        return r;
    }

    // ==================== HostManager 实现 ====================

    HostManager::HostManager() : running_(false) {}

    HostManager::~HostManager() { Stop(); }

    void HostManager::Start()
    {
        running_ = true;
        thread_ = std::make_unique<std::thread>(&HostManager::ProcessLoop, this);
    }

    void HostManager::Stop()
    {
        running_ = false;
        if (thread_ && thread_->joinable())
            thread_->join();
    }

    void HostManager::ProcessLoop()
    {
        while (running_)
        {
            std::this_thread::sleep_for(std::chrono::seconds(60));
            auto now = std::chrono::system_clock::now();
            std::lock_guard<std::mutex> lock(mtx_);
            for (auto it = host_scores_.begin(); it != host_scores_.end();)
            {
                auto age = std::chrono::duration_cast<std::chrono::seconds>(
                               now - it->second.timestamp)
                               .count();
                if (age > 60)
                {
                    std::cout << "Removing stale host: " << it->first << std::endl;
                    it = host_scores_.erase(it);
                }
                else
                    ++it;
            }
        }
    }

    void HostManager::OnDataReceived(const monitor::proto::MonitorInfo &info)
    {
        // 提取主机名
        std::string host_name = ExtractHostName(info);
        if (host_name.empty())
        {
            std::cerr << "Received data with empty server identifier" << std::endl;
            return;
        }

        // 计算评分
        double score = CalcScore(info);
        auto now = std::chrono::system_clock::now();

        // 提取网络速率（MB/s）
        double net_in_rate = 0, net_out_rate = 0;
        if (info.net_info_size() > 0)
        {
            net_in_rate = info.net_info(0).rcv_rate() / (1024.0 * 1024.0);
            net_out_rate = info.net_info(0).send_rate() / (1024.0 * 1024.0);
        }

        // 提取当前采样数据
        PerfSample curr;
        ExtractPerfData(info, net_in_rate, net_out_rate, score, curr);
        PerfSample last = last_perf_samples[host_name]; // 可能为空，ComputeRates 会处理
        RateInfo rates = ComputeRates(curr, last);
        last_perf_samples[host_name] = curr;

        // 更新评分缓存
        {
            std::lock_guard<std::mutex> lock(mtx_);
            host_scores_[host_name] = HostScore{info, score, now};
        }

        // 写入 MySQL
        WriteToMysql(host_name, HostScore{info, score, now},
                     net_in_rate, net_out_rate, rates);

        // 打印详细信息
        std::cout << "\n================== Received Data =================="
                  << std::endl;
        std::cout << "Server: " << host_name << ", Score: " << score << std::endl;

        // CPU 详细信息
        std::cout << "\n--- CPU ---" << std::endl;
        std::cout << "  Usage: " << curr.cpu_percent << "%, "
                  << "User: " << curr.usr_percent << "%, "
                  << "System: " << curr.system_percent << "%" << std::endl;
        std::cout << "  Nice: " << curr.nice_percent << "%, "
                  << "Idle: " << curr.idle_percent << "%, "
                  << "IOWait: " << curr.io_wait_percent << "%" << std::endl;
        std::cout << "  IRQ: " << curr.irq_percent << "%, "
                  << "SoftIRQ: " << curr.soft_irq_percent << "%" << std::endl;
        std::cout << "  Load: " << curr.load_avg_1 << "/" << curr.load_avg_5 << "/"
                  << curr.load_avg_15 << std::endl;

        // 内存详细信息
        std::cout << "\n--- Memory ---" << std::endl;
        std::cout << "  Used: " << curr.mem_used_percent << "%, "
                  << "Total: " << curr.mem_total << " GB" << std::endl;
        std::cout << "  Free: " << curr.mem_free << " GB, "
                  << "Avail: " << curr.mem_avail << " GB" << std::endl;

        // 网络详细信息
        std::cout << "\n--- Network ---" << std::endl;
        std::cout << "  In: " << net_in_rate * 1024 * 1024 << " B/s, "
                  << "Out: " << net_out_rate * 1024 * 1024 << " B/s" << std::endl;
        for (int i = 0; i < info.net_info_size(); ++i)
        {
            const auto &net = info.net_info(i);
            std::cout << "  [" << net.name() << "] Recv: " << net.rcv_rate() << " B/s, "
                      << "Send: " << net.send_rate() << " B/s, "
                      << "Drops: " << net.drop_in() << "/" << net.drop_out()
                      << std::endl;
        }

        // 磁盘详细信息
        std::cout << "\n--- Disk ---" << std::endl;
        float max_disk_util = 0;
        for (int i = 0; i < info.disk_info_size(); ++i)
        {
            const auto &disk = info.disk_info(i);
            std::cout << "  [" << disk.name() << "] "
                      << "Read: " << disk.read_bytes_per_sec() / 1024.0 << " KB/s, "
                      << "Write: " << disk.write_bytes_per_sec() / 1024.0 << " KB/s, "
                      << "Util: " << disk.util_percent() << "%" << std::endl;
            if (disk.util_percent() > max_disk_util)
                max_disk_util = disk.util_percent();
        }
        if (info.disk_info_size() == 0)
        {
            std::cout << "  No disk data" << std::endl;
        }

        // 软中断信息
        std::cout << "\n--- SoftIRQ ---" << std::endl;
        std::cout << "  CPU cores with softirq data: " << info.soft_irq_size()
                  << std::endl;

        // 变化率信息
        std::cout << "  CPU: " << rates.cpu_percent * 100 << "%, "
                  << "Mem: " << rates.mem_used_percent * 100 << "%, "
                  << "Load: " << rates.load_avg_1 * 100 << "%" << std::endl;
        std::cout << "  NetIn: " << rates.net_in_rate * 100 << "%, "
                  << "NetOut: " << rates.net_out_rate * 100 << "%" << std::endl;

        std::cout << "\n--- Database ---" << std::endl;
        std::cout << "  Data saved to MySQL (monitor_db)" << std::endl;
        std::cout << "====================================================\n"
                  << std::endl;
    }

    // 查询接口
    std::unordered_map<std::string, HostScore> HostManager::GetAllHostScores()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return host_scores_;
    }

    std::string HostManager::GetBestHost()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        std::string best_host;
        double best_score = -1;
        for (const auto &[host, data] : host_scores_)
        {
            if (data.score > best_score)
            {
                best_score = data.score;
                best_host = host;
            }
        }
        return best_host;
    }

    // ==================== 评分计算 ====================

    double HostManager::CalcScore(const monitor::proto::MonitorInfo &info)
    {
        // 与原实现完全相同（无变化）
        const double cpu_weight = 0.35;
        const double mem_weight = 0.30;
        const double load_weight = 0.15;
        const double disk_weight = 0.15;
        const double net_weight = 0.05;
        const double load_coefficient = 1.5;
        const double max_bandwidth = 125000000.0;

        double cpu_percent = 0, load_avg_1 = 0, mem_percent = 0;
        double net_recv_rate = 0, net_send_rate = 0, disk_util = 0;
        int cpu_cores = 1;

        if (info.cpu_stat_size() > 0)
        {
            cpu_percent = info.cpu_stat(0).cpu_percent();
            cpu_cores = info.cpu_stat_size() - 1;
            if (cpu_cores < 1)
                cpu_cores = 1;
        }
        if (info.has_cpu_load())
            load_avg_1 = info.cpu_load().load_avg_1();
        if (info.has_mem_info())
            mem_percent = info.mem_info().used_percent();
        if (info.net_info_size() > 0)
        {
            net_recv_rate = info.net_info(0).rcv_rate();
            net_send_rate = info.net_info(0).send_rate();
        }
        if (info.disk_info_size() > 0)
        {
            for (int i = 0; i < info.disk_info_size(); ++i)
            {
                double util = info.disk_info(i).util_percent();
                if (util > disk_util)
                    disk_util = util;
            }
        }

        auto clamp = [](double v)
        { return v < 0 ? 0 : (v > 1 ? 1 : v); };

        double cpu_score = clamp(1.0 - cpu_percent / 100.0);
        double mem_score = clamp(1.0 - mem_percent / 100.0);
        double load_score = clamp(1.0 - load_avg_1 / (cpu_cores * load_coefficient));
        double disk_score = clamp(1.0 - disk_util / 100.0);
        double net_recv_score = clamp(1.0 - net_recv_rate / max_bandwidth);
        double net_send_score = clamp(1.0 - net_send_rate / max_bandwidth);
        double net_score = (net_recv_score + net_send_score) / 2.0;

        double score = cpu_score * cpu_weight + mem_score * mem_weight +
                       load_score * load_weight + disk_score * disk_weight +
                       net_score * net_weight;
        score *= 100.0;
        return score < 0 ? 0 : (score > 100 ? 100 : score);
    }

    // ==================== MySQL 写入（参数已精简） ====================

    void HostManager::WriteToMysql(const std::string &host_name,
                                   const HostScore &host_score,
                                   double net_in_rate,
                                   double net_out_rate,
                                   const RateInfo &rates)
    {
#ifdef ENABLE_MYSQL
        MYSQL *conn = mysql_init(NULL);
        if (!conn)
        {
            std::cerr << "mysql_init failed\n";
            return;
        }
        if (!mysql_real_connect(conn, MYSQL_HOST, MYSQL_USER, MYSQL_PASS, MYSQL_DB, 0, NULL, 0))
        {
            std::cerr << "mysql_real_connect failed: " << mysql_error(conn) << "\n";
            mysql_close(conn);
            return;
        }

        std::time_t t = std::chrono::system_clock::to_time_t(
            std::chrono::time_point_cast<std::chrono::system_clock::duration>(host_score.timestamp));
        std::tm tm_time;
        localtime_r(&t, &tm_time);
        char time_buf[32];
        std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_time);

        const auto &info = host_score.monitor_info;
        // rate 函数用于详细表
        auto rate = [](float now_val, float last_val) -> float
        {
            if (last_val == 0)
                return 0;
            return (now_val - last_val) / last_val;
        };

        // ========== 1. 主表 server_performance ==========
        {
            float total = 0, free_mem = 0, avail = 0, send_rate = 0, rcv_rate = 0;
            float cpu_percent = 0, usr_percent = 0, system_percent = 0;
            float nice_percent = 0, idle_percent = 0, io_wait_percent = 0;
            float irq_percent = 0, soft_irq_percent = 0;
            float load_avg_1 = 0, load_avg_5 = 0, load_avg_15 = 0, mem_used_percent = 0;
            float disk_util_percent = 0;

            if (info.has_mem_info())
            {
                const auto &mem = info.mem_info();
                total = mem.total();
                free_mem = mem.free();
                avail = mem.avail();
                mem_used_percent = mem.used_percent();
            }
            if (info.net_info_size() > 0)
            {
                send_rate = info.net_info(0).send_rate() / 1024.0;
                rcv_rate = info.net_info(0).rcv_rate() / 1024.0;
            }
            if (info.cpu_stat_size() > 0)
            {
                const auto &cpu = info.cpu_stat(0);
                cpu_percent = cpu.cpu_percent();
                usr_percent = cpu.usr_percent();
                system_percent = cpu.system_percent();
                nice_percent = cpu.nice_percent();
                idle_percent = cpu.idle_percent();
                io_wait_percent = cpu.io_wait_percent();
                irq_percent = cpu.irq_percent();
                soft_irq_percent = cpu.soft_irq_percent();
            }
            if (info.has_cpu_load())
            {
                load_avg_1 = info.cpu_load().load_avg_1();
                load_avg_5 = info.cpu_load().load_avg_5();
                load_avg_15 = info.cpu_load().load_avg_15();
            }
            for (int i = 0; i < info.disk_info_size(); ++i)
            {
                float util = info.disk_info(i).util_percent();
                if (util > disk_util_percent)
                    disk_util_percent = util;
            }

            static std::map<std::string, float> last_disk_util;
            float disk_util_percent_rate = 0;
            if (last_disk_util.count(host_name) && last_disk_util[host_name] != 0)
                disk_util_percent_rate = (disk_util_percent - last_disk_util[host_name]) / last_disk_util[host_name];
            last_disk_util[host_name] = disk_util_percent;

            std::ostringstream oss;
            oss << "INSERT INTO server_performance "
                << "(server_name, cpu_percent, usr_percent, system_percent, nice_percent, "
                << "idle_percent, io_wait_percent, irq_percent, soft_irq_percent, "
                << "load_avg_1, load_avg_5, load_avg_15, "
                << "mem_used_percent, total, free, avail, "
                << "disk_util_percent, send_rate, rcv_rate, score, "
                << "cpu_percent_rate, usr_percent_rate, system_percent_rate, nice_percent_rate, "
                << "idle_percent_rate, io_wait_percent_rate, irq_percent_rate, soft_irq_percent_rate, "
                << "load_avg_1_rate, load_avg_5_rate, load_avg_15_rate, "
                << "mem_used_percent_rate, total_rate, free_rate, avail_rate, "
                << "disk_util_percent_rate, send_rate_rate, rcv_rate_rate, timestamp) "
                << "VALUES ('" << host_name << "',"
                << cpu_percent << "," << usr_percent << "," << system_percent << ","
                << nice_percent << "," << idle_percent << "," << io_wait_percent << ","
                << irq_percent << "," << soft_irq_percent << ","
                << load_avg_1 << "," << load_avg_5 << "," << load_avg_15 << ","
                << mem_used_percent << "," << total << "," << free_mem << "," << avail << ","
                << disk_util_percent << "," << send_rate << "," << rcv_rate << ","
                << host_score.score << ","
                << rates.cpu_percent << "," << rates.usr_percent << ","
                << rates.system_percent << "," << rates.nice_percent << ","
                << rates.idle_percent << "," << rates.io_wait_percent << ","
                << rates.irq_percent << "," << rates.soft_irq_percent << ","
                << rates.load_avg_1 << "," << rates.load_avg_5 << "," << rates.load_avg_15 << ","
                << rates.mem_used_percent << "," << rates.mem_total << ","
                << rates.mem_free << "," << rates.mem_avail << ","
                << disk_util_percent_rate << "," << rates.net_in_rate << "," << rates.net_out_rate << ",'"
                << time_buf << "')";
            mysql_query(conn, oss.str().c_str());
        }

        // ========== 2. 网络详细表（与原相同） ==========
        for (int i = 0; i < info.net_info_size(); ++i)
        {
            const auto &net = info.net_info(i);
            std::string net_name = net.name();

            NetDetailSample curr;
            curr.rcv_bytes_rate = net.rcv_rate();
            curr.rcv_packets_rate = net.rcv_packets_rate();
            curr.snd_bytes_rate = net.send_rate();
            curr.snd_packets_rate = net.send_packets_rate();
            curr.err_in = net.err_in();
            curr.err_out = net.err_out();
            curr.drop_in = net.drop_in();
            curr.drop_out = net.drop_out();

            NetDetailSample &last = last_net_samples[host_name][net_name];
            auto rate_u64 = [](uint64_t now_val, uint64_t last_val) -> float
            {
                if (last_val == 0)
                    return 0;
                return static_cast<float>(now_val - last_val) / static_cast<float>(last_val);
            };

            std::ostringstream oss;
            oss << "INSERT INTO server_net_detail "
                << "(server_name, net_name, err_in, err_out, drop_in, drop_out, "
                << "rcv_bytes_rate, rcv_packets_rate, snd_bytes_rate, snd_packets_rate, "
                << "rcv_bytes_rate_rate, rcv_packets_rate_rate, "
                << "snd_bytes_rate_rate, snd_packets_rate_rate, "
                << "err_in_rate, err_out_rate, drop_in_rate, drop_out_rate, "
                << "timestamp) VALUES ('" << host_name << "','" << net_name << "',"
                << curr.err_in << "," << curr.err_out << ","
                << curr.drop_in << "," << curr.drop_out << ","
                << curr.rcv_bytes_rate << "," << curr.rcv_packets_rate << ","
                << curr.snd_bytes_rate << "," << curr.snd_packets_rate << ","
                << rate(curr.rcv_bytes_rate, last.rcv_bytes_rate) << ","
                << rate(curr.rcv_packets_rate, last.rcv_packets_rate) << ","
                << rate(curr.snd_bytes_rate, last.snd_bytes_rate) << ","
                << rate(curr.snd_packets_rate, last.snd_packets_rate) << ","
                << rate_u64(curr.err_in, last.err_in) << ","
                << rate_u64(curr.err_out, last.err_out) << ","
                << rate_u64(curr.drop_in, last.drop_in) << ","
                << rate_u64(curr.drop_out, last.drop_out) << ",'" << time_buf << "')";
            mysql_query(conn, oss.str().c_str());
            last = curr;
        }

        // ========== 3. 软中断详细表（与原相同） ==========
        for (int i = 0; i < info.soft_irq_size(); ++i)
        {
            const auto &sirq = info.soft_irq(i);
            std::string cpu_name = sirq.cpu();

            SoftIrqSample curr;
            curr.hi = sirq.hi();
            curr.timer = sirq.timer();
            curr.net_tx = sirq.net_tx();
            curr.net_rx = sirq.net_rx();
            curr.block = sirq.block();
            curr.irq_poll = sirq.irq_poll();
            curr.tasklet = sirq.tasklet();
            curr.sched = sirq.sched();
            curr.hrtimer = sirq.hrtimer();
            curr.rcu = sirq.rcu();

            SoftIrqSample &last = last_softirq_samples[host_name][cpu_name];
            std::ostringstream oss;
            oss << "INSERT INTO server_softirq_detail "
                << "(server_name, cpu_name, hi, timer, net_tx, net_rx, block, "
                << "irq_poll, tasklet, sched, hrtimer, rcu, "
                << "hi_rate, timer_rate, net_tx_rate, net_rx_rate, block_rate, "
                << "irq_poll_rate, tasklet_rate, sched_rate, hrtimer_rate, rcu_rate, "
                << "timestamp) VALUES ('" << host_name << "','" << cpu_name << "',"
                << curr.hi << "," << curr.timer << "," << curr.net_tx << ","
                << curr.net_rx << "," << curr.block << "," << curr.irq_poll << ","
                << curr.tasklet << "," << curr.sched << "," << curr.hrtimer << ","
                << curr.rcu << ","
                << rate(curr.hi, last.hi) << ","
                << rate(curr.timer, last.timer) << ","
                << rate(curr.net_tx, last.net_tx) << ","
                << rate(curr.net_rx, last.net_rx) << ","
                << rate(curr.block, last.block) << ","
                << rate(curr.irq_poll, last.irq_poll) << ","
                << rate(curr.tasklet, last.tasklet) << ","
                << rate(curr.sched, last.sched) << ","
                << rate(curr.hrtimer, last.hrtimer) << ","
                << rate(curr.rcu, last.rcu) << ",'" << time_buf << "')";
            mysql_query(conn, oss.str().c_str());
            last = curr;
        }

        // ========== 4. 内存详细表（与原相同） ==========
        if (info.has_mem_info())
        {
            const auto &mem = info.mem_info();
            MemDetailSample curr;
            curr.total = mem.total();
            curr.free = mem.free();
            curr.avail = mem.avail();
            curr.buffers = mem.buffers();
            curr.cached = mem.cached();
            curr.swap_cached = mem.swap_cached();
            curr.active = mem.active();
            curr.inactive = mem.inactive();
            curr.active_anon = mem.active_anon();
            curr.inactive_anon = mem.inactive_anon();
            curr.active_file = mem.active_file();
            curr.inactive_file = mem.inactive_file();
            curr.dirty = mem.dirty();
            curr.writeback = mem.writeback();
            curr.anon_pages = mem.anon_pages();
            curr.mapped = mem.mapped();
            curr.kreclaimable = mem.kreclaimable();
            curr.sreclaimable = mem.sreclaimable();
            curr.sunreclaim = mem.sunreclaim();

            MemDetailSample &last = last_mem_samples[host_name];
            std::ostringstream oss;
            oss << "INSERT INTO server_mem_detail "
                << "(server_name, total, free, avail, buffers, cached, swap_cached, "
                << "active, inactive, active_anon, inactive_anon, active_file, inactive_file, "
                << "dirty, writeback, anon_pages, mapped, kreclaimable, sreclaimable, sunreclaim, "
                << "total_rate, free_rate, avail_rate, buffers_rate, cached_rate, swap_cached_rate, "
                << "active_rate, inactive_rate, active_anon_rate, inactive_anon_rate, "
                << "active_file_rate, inactive_file_rate, dirty_rate, writeback_rate, "
                << "anon_pages_rate, mapped_rate, kreclaimable_rate, sreclaimable_rate, "
                << "sunreclaim_rate, timestamp) VALUES ('" << host_name << "',"
                << curr.total << "," << curr.free << "," << curr.avail << ","
                << curr.buffers << "," << curr.cached << "," << curr.swap_cached << ","
                << curr.active << "," << curr.inactive << ","
                << curr.active_anon << "," << curr.inactive_anon << ","
                << curr.active_file << "," << curr.inactive_file << ","
                << curr.dirty << "," << curr.writeback << ","
                << curr.anon_pages << "," << curr.mapped << ","
                << curr.kreclaimable << "," << curr.sreclaimable << "," << curr.sunreclaim << ","
                << rate(curr.total, last.total) << ","
                << rate(curr.free, last.free) << ","
                << rate(curr.avail, last.avail) << ","
                << rate(curr.buffers, last.buffers) << ","
                << rate(curr.cached, last.cached) << ","
                << rate(curr.swap_cached, last.swap_cached) << ","
                << rate(curr.active, last.active) << ","
                << rate(curr.inactive, last.inactive) << ","
                << rate(curr.active_anon, last.active_anon) << ","
                << rate(curr.inactive_anon, last.inactive_anon) << ","
                << rate(curr.active_file, last.active_file) << ","
                << rate(curr.inactive_file, last.inactive_file) << ","
                << rate(curr.dirty, last.dirty) << ","
                << rate(curr.writeback, last.writeback) << ","
                << rate(curr.anon_pages, last.anon_pages) << ","
                << rate(curr.mapped, last.mapped) << ","
                << rate(curr.kreclaimable, last.kreclaimable) << ","
                << rate(curr.sreclaimable, last.sreclaimable) << ","
                << rate(curr.sunreclaim, last.sunreclaim) << ",'" << time_buf << "')";
            mysql_query(conn, oss.str().c_str());
            last = curr;
        }

        // ========== 5. 磁盘详细表（与原相同） ==========
        for (int i = 0; i < info.disk_info_size(); ++i)
        {
            const auto &disk = info.disk_info(i);
            std::string disk_name = disk.name();

            DiskDetailSample curr;
            curr.read_bytes_per_sec = disk.read_bytes_per_sec();
            curr.write_bytes_per_sec = disk.write_bytes_per_sec();
            curr.read_iops = disk.read_iops();
            curr.write_iops = disk.write_iops();
            curr.avg_read_latency_ms = disk.avg_read_latency_ms();
            curr.avg_write_latency_ms = disk.avg_write_latency_ms();
            curr.util_percent = disk.util_percent();

            DiskDetailSample &last = last_disk_samples[host_name][disk_name];
            std::ostringstream oss;
            oss << "INSERT INTO server_disk_detail "
                << "(server_name, disk_name, reads, writes, sectors_read, sectors_written, "
                << "read_time_ms, write_time_ms, io_in_progress, io_time_ms, weighted_io_time_ms, "
                << "read_bytes_per_sec, write_bytes_per_sec, read_iops, write_iops, "
                << "avg_read_latency_ms, avg_write_latency_ms, util_percent, "
                << "read_bytes_per_sec_rate, write_bytes_per_sec_rate, read_iops_rate, write_iops_rate, "
                << "avg_read_latency_ms_rate, avg_write_latency_ms_rate, util_percent_rate, "
                << "timestamp) VALUES ('" << host_name << "','" << disk_name << "',"
                << disk.reads() << "," << disk.writes() << ","
                << disk.sectors_read() << "," << disk.sectors_written() << ","
                << disk.read_time_ms() << "," << disk.write_time_ms() << ","
                << disk.io_in_progress() << "," << disk.io_time_ms() << ","
                << disk.weighted_io_time_ms() << ","
                << curr.read_bytes_per_sec << "," << curr.write_bytes_per_sec << ","
                << curr.read_iops << "," << curr.write_iops << ","
                << curr.avg_read_latency_ms << "," << curr.avg_write_latency_ms << ","
                << curr.util_percent << ","
                << rate(curr.read_bytes_per_sec, last.read_bytes_per_sec) << ","
                << rate(curr.write_bytes_per_sec, last.write_bytes_per_sec) << ","
                << rate(curr.read_iops, last.read_iops) << ","
                << rate(curr.write_iops, last.write_iops) << ","
                << rate(curr.avg_read_latency_ms, last.avg_read_latency_ms) << ","
                << rate(curr.avg_write_latency_ms, last.avg_write_latency_ms) << ","
                << rate(curr.util_percent, last.util_percent) << ",'" << time_buf << "')";
            mysql_query(conn, oss.str().c_str());
            last = curr;
        }

        mysql_close(conn);
#else
        // 未启用 MySQL 时，仅消除未使用参数警告
        (void)host_name;
        (void)host_score;
        (void)net_in_rate;
        (void)net_out_rate;
        (void)rates;
#endif
    }

} // namespace monitor