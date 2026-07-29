/*
 * cpu_softirq_collector.c - 软中断统计数据采集内核模块
 *
 * 功能：
 * 1. 在内核空间分配结构体数组内存，存放所有 CPU 的软中断统计数据
 * 2. 使用内核工作队列（delayed_work）每秒从 kstat_softirqs 读取数据并更新
 * 3. 注册字符设备 /dev/cpu_softirq_monitor，通过 mmap 暴露给用户空间
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/workqueue.h>       /* 工作队列 */
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/cpumask.h>
#include <linux/version.h>
#include <asm/io.h>

#define DEVICE_NAME "cpu_softirq_monitor"
#define CLASS_NAME  "cpu_softirq_monitor"
#define MAX_CPUS    256

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lz");
MODULE_DESCRIPTION("SoftIRQ statistics collector via mmap");
MODULE_VERSION("1.0");

/* 软中断统计结构体 - 与用户空间共享 */
struct softirq_stat {
    char cpu_name[16];
    uint64_t hi;
    uint64_t timer;
    uint64_t net_tx;
    uint64_t net_rx;
    uint64_t block;
    uint64_t irq_poll;
    uint64_t tasklet;
    uint64_t sched;
    uint64_t hrtimer;
    uint64_t rcu;
};

static dev_t dev_num;
static struct cdev softirq_cdev;
static struct class *softirq_class;
static struct device *softirq_device;

static struct softirq_stat *softirq_data;
static unsigned long data_size;

/* 使用 delayed_work 替代 hrtimer */
static struct delayed_work softirq_work;
static int work_interval = HZ;  /* 默认 1 秒 */

/*
 * 更新软中断统计数据
 * 从内核的 kstat_softirqs 读取数据并填充到共享内存
 */
static void update_softirq_stats(void)
{
    int cpu;
    int idx = 0;

    for_each_possible_cpu(cpu) {
        if (idx >= MAX_CPUS)
            break;

        snprintf(softirq_data[idx].cpu_name,
                 sizeof(softirq_data[idx].cpu_name),
                 "CPU%d", cpu);

        softirq_data[idx].hi      = kstat_softirqs_cpu(HI_SOFTIRQ, cpu);
        softirq_data[idx].timer   = kstat_softirqs_cpu(TIMER_SOFTIRQ, cpu);
        softirq_data[idx].net_tx  = kstat_softirqs_cpu(NET_TX_SOFTIRQ, cpu);
        softirq_data[idx].net_rx  = kstat_softirqs_cpu(NET_RX_SOFTIRQ, cpu);
        softirq_data[idx].block   = kstat_softirqs_cpu(BLOCK_SOFTIRQ, cpu);
        softirq_data[idx].irq_poll = kstat_softirqs_cpu(IRQ_POLL_SOFTIRQ, cpu);
        softirq_data[idx].tasklet = kstat_softirqs_cpu(TASKLET_SOFTIRQ, cpu);
        softirq_data[idx].sched   = kstat_softirqs_cpu(SCHED_SOFTIRQ, cpu);
        softirq_data[idx].hrtimer = kstat_softirqs_cpu(HRTIMER_SOFTIRQ, cpu);
        softirq_data[idx].rcu     = kstat_softirqs_cpu(RCU_SOFTIRQ, cpu);

        idx++;
    }

    if (idx < MAX_CPUS)
        softirq_data[idx].cpu_name[0] = '\0';
}

/*
 * 工作队列回调函数
 */
static void softirq_work_handler(struct work_struct *work)
{
    update_softirq_stats();
    /* 重新调度自己，1 秒后再次执行 */
    schedule_delayed_work(&softirq_work, work_interval);
}

/*
 * 文件操作回调
 */
static int softirq_open(struct inode *inode, struct file *file)
{
    pr_info("%s: device opened\n", DEVICE_NAME);
    return 0;
}

static int softirq_release(struct inode *inode, struct file *file)
{
    pr_info("%s: device closed\n", DEVICE_NAME);
    return 0;
}

static int softirq_mmap(struct file *file, struct vm_area_struct *vma)
{
    unsigned long size = vma->vm_end - vma->vm_start;
    unsigned long pfn;
    int ret;

    if (size > data_size) {
        pr_err("%s: mmap size %lu exceeds data size %lu\n",
               DEVICE_NAME, size, data_size);
        return -EINVAL;
    }

    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    pfn = page_to_pfn(virt_to_page(softirq_data));
    ret = remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot);
    if (ret) {
        pr_err("%s: remap_pfn_range failed: %d\n", DEVICE_NAME, ret);
        return ret;
    }

    pr_info("%s: mmap successful, size=%lu\n", DEVICE_NAME, size);
    return 0;
}

static const struct file_operations softirq_fops = {
    .owner   = THIS_MODULE,
    .open    = softirq_open,
    .release = softirq_release,
    .mmap    = softirq_mmap,
};

/*
 * 模块初始化
 */
static int __init softirq_collector_init(void)
{
    int ret;

    pr_info("%s: initializing module\n", DEVICE_NAME);

    data_size = PAGE_ALIGN(sizeof(struct softirq_stat) * MAX_CPUS);
    softirq_data = (struct softirq_stat *)__get_free_pages(GFP_KERNEL | __GFP_ZERO,
                                                           get_order(data_size));
    if (!softirq_data) {
        pr_err("%s: failed to allocate memory\n", DEVICE_NAME);
        return -ENOMEM;
    }
    pr_info("%s: allocated %lu bytes for data\n", DEVICE_NAME, data_size);

    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("%s: failed to allocate device number\n", DEVICE_NAME);
        goto err_free_mem;
    }

    cdev_init(&softirq_cdev, &softirq_fops);
    softirq_cdev.owner = THIS_MODULE;
    ret = cdev_add(&softirq_cdev, dev_num, 1);
    if (ret < 0) {
        pr_err("%s: failed to add cdev\n", DEVICE_NAME);
        goto err_unregister;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    softirq_class = class_create(CLASS_NAME);
#else
    softirq_class = class_create(THIS_MODULE, CLASS_NAME);
#endif
    if (IS_ERR(softirq_class)) {
        pr_err("%s: failed to create class\n", DEVICE_NAME);
        ret = PTR_ERR(softirq_class);
        goto err_cdev_del;
    }

    softirq_device = device_create(softirq_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(softirq_device)) {
        pr_err("%s: failed to create device\n", DEVICE_NAME);
        ret = PTR_ERR(softirq_device);
        goto err_class_destroy;
    }

    /* 初始化工作队列 */
    INIT_DELAYED_WORK(&softirq_work, softirq_work_handler);
    update_softirq_stats();
    schedule_delayed_work(&softirq_work, work_interval);

    pr_info("%s: module loaded successfully\n", DEVICE_NAME);
    return 0;

err_class_destroy:
    class_destroy(softirq_class);
err_cdev_del:
    cdev_del(&softirq_cdev);
err_unregister:
    unregister_chrdev_region(dev_num, 1);
err_free_mem:
    free_pages((unsigned long)softirq_data, get_order(data_size));
    return ret;
}

/*
 * 模块卸载
 */
static void __exit softirq_collector_exit(void)
{
    pr_info("%s: unloading module\n", DEVICE_NAME);

    cancel_delayed_work_sync(&softirq_work);

    device_destroy(softirq_class, dev_num);
    class_destroy(softirq_class);
    cdev_del(&softirq_cdev);
    unregister_chrdev_region(dev_num, 1);

    free_pages((unsigned long)softirq_data, get_order(data_size));

    pr_info("%s: module unloaded\n", DEVICE_NAME);
}

module_init(softirq_collector_init);
module_exit(softirq_collector_exit);