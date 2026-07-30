# ============================================
# Worker 子目录 CMakeLists.txt
# ============================================

cmake_minimum_required(VERSION 3.10)
project(worker)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ===== 选项 =====
option(ENABLE_EBPF "Enable eBPF-based network monitoring" OFF)

# ===== 查找依赖 =====
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf REQUIRED)

# 指定 proto 库路径
set(PROTO_BUILD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../proto/build)

# 如果启用 eBPF，查找 libbpf
if(ENABLE_EBPF)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBBPF libbpf)
    else()
        find_library(LIBBPF_LIBRARY bpf)
        find_path(LIBBPF_INCLUDE_DIR bpf/libbpf.h)
    endif()

    if(NOT LIBBPF_FOUND AND NOT LIBBPF_LIBRARY)
        message(WARNING "libbpf not found, disabling eBPF support")
        set(ENABLE_EBPF OFF)
    else()
        message(STATUS "eBPF support enabled")
    endif()
endif()

# ===== 源文件列表 =====
set(WORKER_SOURCES
    src/main.cpp
    src/monitor/metric_collector.cpp
    src/monitor/cpu_load_monitor.cpp
    src/monitor/cpu_softirq_monitor.cpp
    src/monitor/cpu_stat_monitor.cpp
    src/monitor/mem_monitor.cpp
    src/monitor/disk_monitor.cpp
    src/monitor/host_info_monitor.cpp
    src/monitor/user_monitor.cpp
    src/rpc/monitor_pusher.cpp
    src/utils/read_file.cpp
)

# 根据 eBPF 支持选择网络监控实现
if(ENABLE_EBPF)
    list(APPEND WORKER_SOURCES src/monitor/net_ebpf_monitor.cpp)
    message(STATUS "Using eBPF network monitor")
else()
    list(APPEND WORKER_SOURCES src/monitor/net_monitor.cpp)
    message(STATUS "Using /proc/net/dev network monitor")
endif()

# ===== 可执行文件 =====
add_executable(worker ${WORKER_SOURCES})

# ===== 头文件路径 =====
target_include_directories(worker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROTO_BUILD_DIR}
)

# 如果启用 eBPF，添加 ebpf 头文件路径
if(ENABLE_EBPF)
    target_include_directories(worker PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ebpf/.output   # net_stats.skel.h
        ${LIBBPF_INCLUDE_DIR}                           # libbpf 头文件
    )
endif()

# ===== 链接库 =====
target_link_libraries(worker
    monitor_proto
    gRPC::grpc++
    gRPC::grpc
    protobuf::libprotobuf
    pthread
    dl
)

# 如果启用 eBPF，链接 libbpf
if(ENABLE_EBPF)
    if(LIBBPF_LIBRARY)
        target_link_libraries(worker ${LIBBPF_LIBRARY})
    elseif(LIBBPF_LDFLAGS)
        target_link_libraries(worker ${LIBBPF_LDFLAGS})
    else()
        # 尝试默认库名
        target_link_libraries(worker bpf)
    endif()
endif()

# ===== 编译定义 =====
if(ENABLE_EBPF)
    target_compile_definitions(worker PRIVATE ENABLE_EBPF=1)
endif()

message(STATUS "Worker configured")