#include "monitor/host_info_monitor.h"
#include "monitor_info.pb.h"
#include <arpa/inet.h>
#include <dirent.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace monitor
{
    std::string HostInfoMonitor::GetHostname()
    {
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0)
        {
            return std::string(hostname);
        }
        return "unknown";
    }

    std::string HostInfoMonitor::GetPrimaryIpAddress()
    {
        struct ifaddrs *ifaddr = nullptr;
        struct ifaddrs *ifa = nullptr;
        std::string primary_ip = "unknown";

        if (getifaddrs(&ifaddr) == -1)
        {
            return primary_ip;
        }

        // 遍历所有网络接口
        for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr == nullptr)
                continue;

            // 只考虑 IPv4 地址
            if (ifa->ifa_addr->sa_family != AF_INET)
                continue;
            // 跳过回环接口
            if (ifa->ifa_flags & IFF_LOOPBACK)
                continue;
            // 跳过虚拟接口
            std::string ifname(ifa->ifa_name);
            if (ifname.find("docker") == 0 || ifname.find("veth") == 0 || ifname.find("lo") == 0 || ifname.find("br-") == 0 || ifname.find("virbr") == 0 || ifname.find("vmnet") == 0 || ifname.find("vboxnet") == 0)
                continue;

            // 获取 IP 地址
            struct sockaddr_in *addr = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
            char ip[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip)) != nullptr)
            {
                primary_ip = std::string(ip);
                break; // 找到第一个非回环、非虚拟接口的 IPv4
            }
        }

        freeifaddrs(ifaddr);
        return primary_ip;
    }


    void HostInfoMonitor::UpdateOnce(monitor::proto::MonitorInfo *monitor_info)
    {
        if(!monitor_info)
        {
            return;
        }

        if(!info_cached_)
        {
            cached_hostname_ = GetHostname();
            cached_ip_ = GetPrimaryIpAddress();
            info_cached_ = true;
        }

        auto host_info = monitor_info->mutable_host_info();
        host_info->set_hostname(cached_hostname_);
        host_info->set_ip_address(cached_ip_);
    }

    void HostInfoMonitor::Stop()
    {
        // No resources to clean up in this implementation
    }
}