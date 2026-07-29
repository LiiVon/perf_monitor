# 内核模块 Makefile

# 模块名称
obj-m += cpu_stat_collector.o
obj-m += cpu_softirq_collector.o

# 内核源码路径
KDIR ?= /lib/modules/$(shell uname -r)/build

# 当前目录
PWD := $(shell pwd)

# 默认目标：编译模块
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# 清理
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

# 安装所有模块
install: install-softirq install-cpustat

# 安装软中断模块
install-softirq:
	sudo insmod softirq_collector.ko
	@echo "Module loaded. Device: /dev/cpu_softirq_monitor"

# 安装 CPU 状态模块
install-cpustat:
	sudo insmod cpu_stat_collector.ko
	@echo "Module loaded. Device: /dev/cpu_stat_monitor"

# 卸载所有模块
uninstall: uninstall-softirq uninstall-cpustat

# 卸载软中断模块
uninstall-softirq:
	-sudo rmmod softirq_collector
	@echo "softirq_collector module unloaded."

# 卸载 CPU 状态模块
uninstall-cpustat:
	-sudo rmmod cpu_stat_collector
	@echo "cpu_stat_collector module unloaded."

# 查看模块信息
info:
	modinfo softirq_collector.ko
	modinfo cpu_stat_collector.ko

# 查看内核日志
log:
	sudo dmesg | tail -30

.PHONY: all clean install install-softirq install-cpustat uninstall uninstall-softirq uninstall-cpustat info log

