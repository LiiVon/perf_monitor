cmake_minimum_required(VERSION 3.10)
project(worker1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROTO_BUILD_DIR /home/lz/lz_ws/perf_monitor/build/proto)

find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf REQUIRED)

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

add_executable(worker1 ${WORKER_SOURCES})

target_include_directories(worker1 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROTO_BUILD_DIR}
)

# ===== 直接链接库文件（无需 link_directories） =====
target_link_libraries(worker1
    ${PROTO_BUILD_DIR}/libmonitor_proto.a
    gRPC::grpc++
    gRPC::grpc
    protobuf::libprotobuf
    pthread
    dl
)

message(STATUS "Worker configured: ${CMAKE_CURRENT_SOURCE_DIR}")