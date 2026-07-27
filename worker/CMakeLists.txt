# Worker - 工作者服务器
# 部署在被监控机器上，采集性能数据并推送给管理者服务器


# # eBPF 支持选项
# option(ENABLE_EBPF "Enable eBPF-based network monitoring" ON)

# # 查找 libbpf
# if(ENABLE_EBPF)
#     find_library(LIBBPF_LIBRARY bpf PATHS /usr/lib64 /usr/lib/x86_64-linux-gnu /usr/local/lib)
#     find_library(LIBELF_LIBRARY elf)
#     find_library(LIBZ_LIBRARY z)
    
#     if(EXISTS "/usr/include/bpf/libbpf.h")
#         set(LIBBPF_INCLUDE_DIR "/usr/include")
#     else()
#         find_path(LIBBPF_INCLUDE_DIR bpf/libbpf.h)
#     endif()
    
#     if(LIBBPF_LIBRARY AND LIBELF_LIBRARY AND LIBZ_LIBRARY AND LIBBPF_INCLUDE_DIR)
#         set(EBPF_FOUND TRUE)
#         message(STATUS "eBPF support enabled")
#     else()
#         set(EBPF_FOUND FALSE)
#         message(WARNING "eBPF dependencies not found, disabling eBPF support")
#     endif()
# endif()

# # eBPF skeleton 生成
# if(EBPF_FOUND)
#     set(EBPF_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/ebpf)
#     set(EBPF_SKEL_H ${CMAKE_CURRENT_SOURCE_DIR}/include/monitor/net_stats.skel.h)
    
#     add_custom_command(
#         OUTPUT ${EBPF_SKEL_H}
#         COMMAND make -C ${EBPF_SRC_DIR}
#         COMMAND ${CMAKE_COMMAND} -E copy ${EBPF_SRC_DIR}/.output/net_stats.skel.h ${EBPF_SKEL_H}
#         DEPENDS ${EBPF_SRC_DIR}/net_stats.bpf.c
#         COMMENT "Building eBPF program and generating skeleton"
#     )
#     add_custom_target(ebpf_skeleton DEPENDS ${EBPF_SKEL_H})
# endif()


# 单独测试 worker作为一个项目
cmake_minimum_required(VERSION 3.10)
project(worker) 
# 设置 C++ 标准（使用 C++17，因为 gRPC 和 Protobuf 都需要）
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 指定 proto 库的路径
set(PROTO_BUILD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../proto/build)
# 告诉 CMake 去这个目录找库文件
link_directories(${PROTO_BUILD_DIR})

# 告诉 CMake 去这个目录找头文件（proto 生成的头文件在 build 目录下）
include_directories(${PROTO_BUILD_DIR})

# 查找 gRPC 和 Protobuf（worker 需要链接它们）
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf REQUIRED)


# 源文件列表
set(WORKER_SOURCES
    src/main.cpp
    # src/monitor/metric_collector.cpp
    src/monitor/cpu_load_monitor.cpp
    src/monitor/cpu_softirq_monitor.cpp
    # src/monitor/cpu_stat_monitor.cpp
    # src/monitor/mem_monitor.cpp
    # src/monitor/disk_monitor.cpp
    # src/monitor/host_info_monitor.cpp
    # src/rpc/monitor_pusher.cpp
    src/utils/read_file.cpp
)

# # 根据 eBPF 支持选择网络监控实现
# if(EBPF_FOUND)
#     list(APPEND WORKER_SOURCES src/monitor/net_ebpf_monitor.cpp)
# else()
#     list(APPEND WORKER_SOURCES src/monitor/net_monitor.cpp)
# endif()

# 可执行文件: worker (工作者服务器)
add_executable(worker ${WORKER_SOURCES})

# target_include_directories(worker PUBLIC
#     ${PROJECT_SOURCE_DIR}/worker/include
    
# )

target_include_directories(worker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include   # worker 自己的头文件（utils/、monitor/、rpc/）
    ${PROTO_BUILD_DIR}                    # proto 生成的头文件（*.pb.h, *.grpc.pb.h）
)

# 基础链接库
# set(WORKER_LIBS monitor_proto gRPC::grpc++)
target_link_libraries(worker
    monitor_proto                        # 你的 proto 静态库（libmonitor_proto.a）
    gRPC::grpc++                         # gRPC C++ 库
    gRPC::grpc                           # gRPC 核心库
    protobuf::libprotobuf                # Protobuf 库
    pthread                              # gRPC 需要线程库
    dl                                   # 动态链接库（某些系统需要）
)

# # eBPF 依赖
# if(EBPF_FOUND)
#     add_dependencies(worker ebpf_skeleton)
#     target_compile_definitions(worker PRIVATE ENABLE_EBPF=1)
#     target_include_directories(worker PRIVATE ${LIBBPF_INCLUDE_DIR})
#     list(APPEND WORKER_LIBS ${LIBBPF_LIBRARY} ${LIBELF_LIBRARY} ${LIBZ_LIBRARY})
# endif()

# target_link_libraries(worker PUBLIC ${WORKER_LIBS})

# # 内核模块构建（可选）
# add_custom_target(kernel_modules
#     COMMAND make -C ${CMAKE_CURRENT_SOURCE_DIR}/src/kmod
#     COMMENT "Building kernel modules"
# )

# # eBPF 程序构建（可选）
# if(EBPF_FOUND)
#     add_custom_target(ebpf_programs
#         COMMAND make -C ${CMAKE_CURRENT_SOURCE_DIR}/src/ebpf
#         COMMENT "Building eBPF programs"
#     )
# endif()
