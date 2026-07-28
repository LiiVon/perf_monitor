cmake_minimum_required(VERSION 3.10.2)
project(monitor_system LANGUAGES CXX)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ===== 构建选项 =====
option(BUILD_MANAGER "Build manager server (requires MySQL)" ON)
option(ENABLE_EBPF "Enable eBPF-based network monitoring" OFF)  # 默认关闭，按需开启

# ===== 统一查找 gRPC 和 Protobuf（所有子目录共享） =====
find_package(gRPC CONFIG QUIET)
if(NOT gRPC_FOUND)
    # 手动查找（兼容旧系统）
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

find_package(protobuf CONFIG QUIET)
if(NOT protobuf_FOUND)
    find_package(Protobuf REQUIRED)
endif()

# ===== 子目录（按依赖顺序） =====
# 1. Proto：必须先编译，生成 libmonitor_proto.a 和头文件
add_subdirectory(proto)

# 2. Worker：依赖 monitor_proto
add_subdirectory(worker)

# 3. Manager：依赖 monitor_proto（可选）
if(BUILD_MANAGER)
    add_subdirectory(manager)
endif()

# ===== 打印构建摘要 =====
message(STATUS "========================================")
message(STATUS "monitor_system build configuration:")
message(STATUS "  BUILD_MANAGER: ${BUILD_MANAGER}")
message(STATUS "  ENABLE_EBPF:   ${ENABLE_EBPF}")
message(STATUS "========================================")