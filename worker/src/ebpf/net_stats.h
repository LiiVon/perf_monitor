// net_stats.h eBPF 程序和用户空间程序共享的数据结构

#ifndef __NET_STATS_H__
#define __NET_STATS_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

    struct net_stats
    {
        uint64_t send_bytes;
        uint64_t rcv_bytes;
        uint64_t send_packets;
        uint64_t rcv_packets;

        // 下面的是在 proc/net/dev 中获取的错误和丢弃统计信息
        // uint64_t err_in;
        // uint64_t err_out;
        // uint64_t drop_in;
        // uint64_t drop_out;
    };

// Map
#define NET_STATS_MAP_NAME "net_stats_map"

// 最大网卡数量
#define MAX_NET_DEVICES 64

#ifdef __cplusplus
}
#endif

#endif