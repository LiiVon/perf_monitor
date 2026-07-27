#pragma once
#include <stdint.h>

// 内核模块与用户空间共享的数据结构定义
#ifdef __cplusplus
extern "C"
{
#endif
    // CPU 负载结构体
    struct cpu_load
    {
        float load_avg_1;  // 1分钟平均负载
        float load_avg_5;  // 5分钟平均负载
        float load_avg_15; // 15分钟平均负载
    };
    
    // 软中断统计结构体
    struct softirq_stat
    {
        char cpu_name[16]; // CPU 名称
        uint64_t hi;       // 高优先级软中断时间
        uint64_t timer;    // 定时器软中断时间
        uint64_t net_tx;   // 网络发送软中断时间
        uint64_t net_rx;   // 网络接收软中断时间
        uint64_t block;    // 块设备软中断时间
        uint64_t irq_poll; // 中断轮询软中断时间
        uint64_t tasklet;  // 任务软中断时间
        uint64_t sched;    // 调度软中断时间
        uint64_t hrtimer;  // 高精度定时器软中断时间
        uint64_t rcu;      // RCU软中断时间
    };

    // CPU 使用率结构体
    struct cpu_stat
    {
        char cpu_name[16];   // CPU 名称
        uint64_t user;       // 用户态时间
        uint64_t nice;       // 用户态低优先级时间
        uint64_t system;     // 内核态时间
        uint64_t idle;       // 空闲时间
        uint64_t iowait;     // I/O等待时间
        uint64_t irq;        // 硬中断时间
        uint64_t softirq;    // 软中断时间
        uint64_t steal;      // 被虚拟机抢占的时间
        uint64_t guest;      // 运行虚拟机的时间
        uint64_t guest_nice; // 运行低优先级虚拟机的时间
    };

#ifdef __cplusplus
}
#endif