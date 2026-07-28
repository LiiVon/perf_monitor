# ============================================
# Worker 子目录 CMakeLists.txt
# 被顶层 CMake 通过 add_subdirectory(worker) 调用
# ============================================

# 不需要 project() 和 find_package，顶层已经做了

# 源文件列表（注意：使用绝对路径相对于当前目录）
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
    src/monitor/net_monitor.cpp
    src/rpc/monitor_pusher.cpp
    src/utils/read_file.cpp
)

# 可执行文件
add_executable(worker ${WORKER_SOURCES})

# 头文件路径
target_include_directories(worker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include          # worker 自己的头文件
    ${CMAKE_CURRENT_SOURCE_DIR}/../proto/build   # proto 生成的头文件（如果顶层未传递）
)

# 链接库（monitor_proto 由顶层 proto 子目录构建）
target_link_libraries(worker
    monitor_proto              # 由 proto/CMakeLists.txt 构建
    gRPC::grpc++
    gRPC::grpc
    protobuf::libprotobuf
    pthread
    dl
)

# ===== eBPF 支持（可选，默认关闭） =====
if(ENABLE_EBPF)
    # 如果启用，添加 eBPF 相关源文件和依赖
    # 此处省略，保持与原项目一致
endif()

message(STATUS "Worker configured: worker")