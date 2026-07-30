# eBPF 程序编译 Makefile

CLANG ?= clang
BPFTOOL ?= bpftool
ARCH := $(shell uname -m | sed 's/x86_64/x86/' | sed 's/aarch64/arm64/')

# vmlinux.h 位置（使用当前目录下生成的 vmlinux.h）
VMLINUX_H ?= ./vmlinux.h

# 输出目录
OUTPUT_DIR := .output
SKEL_DIR := $(OUTPUT_DIR)

# BPF 程序源文件
BPF_SRCS := net_stats.bpf.c
BPF_OBJS := $(patsubst %.bpf.c,$(OUTPUT_DIR)/%.bpf.o,$(BPF_SRCS))
BPF_SKELS := $(patsubst %.bpf.c,$(SKEL_DIR)/%.skel.h,$(BPF_SRCS))

# ========== 显式添加 libbpf 头文件路径 ==========
BPF_CFLAGS := -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH)
BPF_CFLAGS += -I.                              # 当前目录（vmlinux.h 所在）
BPF_CFLAGS += -I/usr/include                   # 系统头文件
BPF_CFLAGS += -I/usr/include/bpf               # libbpf 头文件
BPF_CFLAGS += -Wno-missing-declarations   	# 忽略缺失声明的警告


.PHONY: all clean info

all: $(OUTPUT_DIR) $(BPF_SKELS)

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

# 如果 vmlinux.h 不存在，提示用户生成
vmlinux.h:
	@echo "vmlinux.h not found! Generating using bpftool..."
	@if [ -f /sys/kernel/btf/vmlinux ]; then \
		$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h; \
	else \
		echo "ERROR: /sys/kernel/btf/vmlinux not found. Please install linux-tools or generate vmlinux.h manually."; \
		exit 1; \
	fi

# 编译 BPF 程序（依赖 vmlinux.h）
$(OUTPUT_DIR)/%.bpf.o: %.bpf.c vmlinux.h | $(OUTPUT_DIR)
	$(CLANG) $(BPF_CFLAGS) -c $< -o $@

# 生成 skeleton 头文件
$(SKEL_DIR)/%.skel.h: $(OUTPUT_DIR)/%.bpf.o | $(OUTPUT_DIR)
	$(BPFTOOL) gen skeleton $< > $@

clean:
	rm -rf $(OUTPUT_DIR)
	rm -f vmlinux.h

# 显示编译信息（调试用）
info:
	@echo "CLANG: $(CLANG)"
	@echo "BPFTOOL: $(BPFTOOL)"
	@echo "ARCH: $(ARCH)"
	@echo "VMLINUX_H: $(VMLINUX_H)"
	@echo "BPF_CFLAGS: $(BPF_CFLAGS)"
	@echo "BPF_OBJS: $(BPF_OBJS)"
	@echo "BPF_SKELS: $(BPF_SKELS)"