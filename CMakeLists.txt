cmake_minimum_required(VERSION 3.10.2)
project(perf_monitor LANGUAGES CXX)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_CXX_STANDARD 17)

option(BUILD_MANAGER "Build manager server (requires MySQL)" ON)

# ===== 统一查找 gRPC（避免子目录重复定义） =====
find_package(gRPC CONFIG QUIET)
if(NOT gRPC_FOUND)
    find_library(GRPC_LIBRARY grpc)
    find_library(GRPCPP_LIBRARY grpc++)
    find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin)
    if(GRPC_LIBRARY AND GRPCPP_LIBRARY)
        add_library(grpc_lib INTERFACE)
        target_link_libraries(grpc_lib INTERFACE ${GRPC_LIBRARY})
        add_library(gRPC::grpc ALIAS grpc_lib)

        add_library(grpcpp_lib INTERFACE)
        target_link_libraries(grpcpp_lib INTERFACE ${GRPCPP_LIBRARY})
        add_library(gRPC::grpc++ ALIAS grpcpp_lib)
    endif()
endif()

# Proto 定义（必须先编译）
add_subdirectory(proto)

# Worker - 工作者服务器，部署在被监控机器上，采集性能数据并推送给管理者
add_subdirectory(worker)

# Manager - 管理者服务器，部署在管理端，接收数据、存储、提供查询服务
if(BUILD_MANAGER)
    add_subdirectory(manager)
endif()
