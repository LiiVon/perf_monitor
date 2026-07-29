// SPDX-License-Identifier: GPL-2.0
/*
 * cpu_stat_collector.c - CPU 状态统计数据采集内核模块
 * 使用 delayed_work 替代 hrtimer/timer，兼容性更好
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/workqueue.h> /* 工作队列 */
#include <linux/kernel_stat.h>
#include <linux/cpumask.h>
#include <linux/version.h>
#include <linux/tick.h>
#include <asm/io.h>

#define DEVICE_NAME "cpu_stat_monitor"
#define CLASS_NAME "cpu_stat_monitor"
#define MAX_CPUS 256

static inline u64 nsec_to_jiffies(u64 nsec)
{
    return div_u64(nsec, NSEC_PER_SEC / HZ);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lz");
MODULE_DESCRIPTION("CPU statistics collector via mmap");
MODULE_VERSION("1.0");

struct cpu_stat
{
    char cpu_name[16];
    uint64_t user;
    uint64_t nice;
    uint64_t system;
    uint64_t idle;
    uint64_t iowait;
    uint64_t irq;
    uint64_t softirq;
    uint64_t steal;
    uint64_t guest;
    uint64_t guest_nice;
};

static dev_t dev_num;
static struct cdev cpu_stat_cdev;
static struct class *cpu_stat_class;
static struct device *cpu_stat_device;
static struct cpu_stat *cpu_stat_data;
static unsigned long data_size;

/* 使用 delayed_work 替代定时器 */
static struct delayed_work cpu_stat_work;
static int work_interval = HZ; /* 默认 1 秒 */

/* ===== CPU 时间获取 ===== */
static u64 cpu_stat_get_idle_time(int cpu)
{
    u64 idle_time = get_cpu_idle_time_us(cpu, NULL);
    if (idle_time == -1ULL)
    {
        idle_time = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
        idle_time = nsec_to_jiffies(idle_time);
    }
    else
    {
        idle_time = usecs_to_jiffies(idle_time);
    }
    return idle_time;
}

static u64 cpu_stat_get_iowait_time(int cpu)
{
    u64 iowait_time = get_cpu_iowait_time_us(cpu, NULL);
    if (iowait_time == -1ULL)
    {
        iowait_time = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
        iowait_time = nsec_to_jiffies(iowait_time);
    }
    else
    {
        iowait_time = usecs_to_jiffies(iowait_time);
    }
    return iowait_time;
}

/* ===== 数据更新 ===== */
static void update_cpu_stats(void)
{
    int cpu, idx = 0;
    struct kernel_cpustat *kcs;

    for_each_possible_cpu(cpu)
    {
        if (idx >= MAX_CPUS)
            break;
        kcs = &kcpustat_cpu(cpu);
        snprintf(cpu_stat_data[idx].cpu_name, sizeof(cpu_stat_data[idx].cpu_name), "cpu%d", cpu);
        cpu_stat_data[idx].user = nsec_to_jiffies(kcs->cpustat[CPUTIME_USER]);
        cpu_stat_data[idx].nice = nsec_to_jiffies(kcs->cpustat[CPUTIME_NICE]);
        cpu_stat_data[idx].system = nsec_to_jiffies(kcs->cpustat[CPUTIME_SYSTEM]);
        cpu_stat_data[idx].idle = cpu_stat_get_idle_time(cpu);
        cpu_stat_data[idx].iowait = cpu_stat_get_iowait_time(cpu);
        cpu_stat_data[idx].irq = nsec_to_jiffies(kcs->cpustat[CPUTIME_IRQ]);
        cpu_stat_data[idx].softirq = nsec_to_jiffies(kcs->cpustat[CPUTIME_SOFTIRQ]);
        cpu_stat_data[idx].steal = nsec_to_jiffies(kcs->cpustat[CPUTIME_STEAL]);
        cpu_stat_data[idx].guest = nsec_to_jiffies(kcs->cpustat[CPUTIME_GUEST]);
        cpu_stat_data[idx].guest_nice = nsec_to_jiffies(kcs->cpustat[CPUTIME_GUEST_NICE]);
        idx++;
    }
    if (idx < MAX_CPUS)
        cpu_stat_data[idx].cpu_name[0] = '\0';
}

/* ===== 工作队列回调 ===== */
static void cpu_stat_work_handler(struct work_struct *work)
{
    update_cpu_stats();
    /* 重新调度自己 */
    schedule_delayed_work(&cpu_stat_work, work_interval);
}

/* ===== 文件操作 ===== */
static int cpu_stat_open(struct inode *inode, struct file *file)
{
    pr_info("%s: device opened\n", DEVICE_NAME);
    return 0;
}

static int cpu_stat_release(struct inode *inode, struct file *file)
{
    pr_info("%s: device closed\n", DEVICE_NAME);
    return 0;
}

static int cpu_stat_mmap(struct file *file, struct vm_area_struct *vma)
{
    unsigned long size = vma->vm_end - vma->vm_start;
    unsigned long pfn;
    int ret;

    if (size > data_size)
    {
        pr_err("%s: mmap size %lu exceeds data size %lu\n", DEVICE_NAME, size, data_size);
        return -EINVAL;
    }
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    pfn = page_to_pfn(virt_to_page(cpu_stat_data));
    ret = remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot);
    if (ret)
    {
        pr_err("%s: remap_pfn_range failed: %d\n", DEVICE_NAME, ret);
        return ret;
    }
    pr_info("%s: mmap successful, size=%lu\n", DEVICE_NAME, size);
    return 0;
}

static const struct file_operations cpu_stat_fops = {
    .owner = THIS_MODULE,
    .open = cpu_stat_open,
    .release = cpu_stat_release,
    .mmap = cpu_stat_mmap,
};

/* ===== 模块初始化 ===== */
static int __init cpu_stat_collector_init(void)
{
    int ret;

    pr_info("%s: initializing module\n", DEVICE_NAME);

    data_size = PAGE_ALIGN(sizeof(struct cpu_stat) * MAX_CPUS);
    cpu_stat_data = (struct cpu_stat *)__get_free_pages(GFP_KERNEL | __GFP_ZERO, get_order(data_size));
    if (!cpu_stat_data)
    {
        pr_err("%s: failed to allocate memory\n", DEVICE_NAME);
        return -ENOMEM;
    }
    pr_info("%s: allocated %lu bytes for data\n", DEVICE_NAME, data_size);

    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0)
    {
        pr_err("%s: failed to allocate device number\n", DEVICE_NAME);
        goto err_free_mem;
    }

    cdev_init(&cpu_stat_cdev, &cpu_stat_fops);
    cpu_stat_cdev.owner = THIS_MODULE;
    ret = cdev_add(&cpu_stat_cdev, dev_num, 1);
    if (ret < 0)
    {
        pr_err("%s: failed to add cdev\n", DEVICE_NAME);
        goto err_unregister;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    cpu_stat_class = class_create(CLASS_NAME);
#else
    cpu_stat_class = class_create(THIS_MODULE, CLASS_NAME);
#endif
    if (IS_ERR(cpu_stat_class))
    {
        pr_err("%s: failed to create class\n", DEVICE_NAME);
        ret = PTR_ERR(cpu_stat_class);
        goto err_cdev_del;
    }

    cpu_stat_device = device_create(cpu_stat_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(cpu_stat_device))
    {
        pr_err("%s: failed to create device\n", DEVICE_NAME);
        ret = PTR_ERR(cpu_stat_device);
        goto err_class_destroy;
    }

    /* ===== 初始化工作队列 ===== */
    INIT_DELAYED_WORK(&cpu_stat_work, cpu_stat_work_handler);
    update_cpu_stats();
    schedule_delayed_work(&cpu_stat_work, work_interval);

    pr_info("%s: module loaded successfully\n", DEVICE_NAME);
    return 0;

err_class_destroy:
    class_destroy(cpu_stat_class);
err_cdev_del:
    cdev_del(&cpu_stat_cdev);
err_unregister:
    unregister_chrdev_region(dev_num, 1);
err_free_mem:
    free_pages((unsigned long)cpu_stat_data, get_order(data_size));
    return ret;
}

/* ===== 模块卸载 ===== */
static void __exit cpu_stat_collector_exit(void)
{
    pr_info("%s: unloading module\n", DEVICE_NAME);

    /* 取消工作队列 */
    cancel_delayed_work_sync(&cpu_stat_work);

    device_destroy(cpu_stat_class, dev_num);
    class_destroy(cpu_stat_class);
    cdev_del(&cpu_stat_cdev);
    unregister_chrdev_region(dev_num, 1);
    free_pages((unsigned long)cpu_stat_data, get_order(data_size));

    pr_info("%s: module unloaded\n", DEVICE_NAME);
}

module_init(cpu_stat_collector_init);
module_exit(cpu_stat_collector_exit);