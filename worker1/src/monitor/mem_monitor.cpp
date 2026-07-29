#include "monitor/mem_monitor.h"
#include "utils/read_file.h"
#include <cstring>
namespace monitor
{
    static const char *MEM_INFO_FILE = "/proc/meminfo";
    static constexpr float KBToGB = 1000.0f * 1000.0f;  // kB → GB

    void MemMonitor::UpdateOnce(monitor::proto::MonitorInfo *monitor_info)
    {
        ReadFile rf(MEM_INFO_FILE);
        std::vector<std::string> mem_datas;
        MemInfo mem_info;

        while (true)
        {
            mem_datas.clear();
            if (!rf.ReadLine(&mem_datas))
            {
                break;
            }

            if (mem_datas.size() < 2)
            {
                continue;
            }

            std::string key = mem_datas[0];
            int64_t value = std::stoll(mem_datas[1]);

            if (key == "MemTotal:")
                mem_info.total = value;
            else if (key == "MemFree:")
                mem_info.free = value;
            else if (key == "MemAvailable:")
                mem_info.available = value;
            else if (key == "Buffers:")
                mem_info.buffers = value;
            else if (key == "Cached:")
                mem_info.cached = value;
            else if (key == "SwapCached:")
                mem_info.swap_cached = value;
            else if (key == "Active:")
                mem_info.active = value;
            else if (key == "Inactive:")
                mem_info.in_active = value;
            else if (key == "Active(anon):")
                mem_info.active_anon = value;
            else if (key == "Inactive(anon):")
                mem_info.inactive_anon = value;
            else if (key == "Active(file):")
                mem_info.active_file = value;
            else if (key == "Inactive(file):")
                mem_info.inactive_file = value;
            else if (key == "Dirty:")
                mem_info.dirty = value;
            else if (key == "Writeback:")
                mem_info.writeback = value;
            else if (key == "AnonPages:")
                mem_info.anon_pages = value;
            else if (key == "Mapped:")
                mem_info.mapped = value;
            else if (key == "KReclaimable:")
                mem_info.kReclaimable = value;
            else if (key == "SReclaimable:")
                mem_info.sReclaimable = value;
            else if (key == "SUnreclaim:")
                mem_info.sUnreclaim = value;
        }

        auto mem_detail = monitor_info->mutable_mem_info();
        mem_detail->set_used_percent(((mem_info.total - mem_info.free) * 100.0) / mem_info.total);
        mem_detail->set_total(mem_info.total / KBToGB);
        mem_detail->set_free(mem_info.free / KBToGB);
        mem_detail->set_avail(mem_info.available / KBToGB);
        mem_detail->set_buffers(mem_info.buffers / KBToGB);
        mem_detail->set_cached(mem_info.cached / KBToGB);
        mem_detail->set_swap_cached(mem_info.swap_cached / KBToGB);
        mem_detail->set_active(mem_info.active / KBToGB);
        mem_detail->set_inactive(mem_info.in_active / KBToGB);
        mem_detail->set_active_anon(mem_info.active_anon / KBToGB);
        mem_detail->set_inactive_anon(mem_info.inactive_anon / KBToGB);
        mem_detail->set_active_file(mem_info.active_file / KBToGB);
        mem_detail->set_inactive_file(mem_info.inactive_file / KBToGB);
        mem_detail->set_dirty(mem_info.dirty / KBToGB);
        mem_detail->set_writeback(mem_info.writeback / KBToGB);
        mem_detail->set_anon_pages(mem_info.anon_pages / KBToGB);
        mem_detail->set_mapped(mem_info.mapped / KBToGB);
        mem_detail->set_kreclaimable(mem_info.kReclaimable / KBToGB);
        mem_detail->set_sreclaimable(mem_info.sReclaimable / KBToGB);
        mem_detail->set_sunreclaim(mem_info.sUnreclaim / KBToGB);
    }

    void MemMonitor::Stop()
    {
    }
}
