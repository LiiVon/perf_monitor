# # Manager - 管理者服务器
# # 部署在管理端，接收工作者推送的数据、存储到数据库、提供查询服务



cmake_minimum_required(VERSION 3.10)
project(Manager)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 gRPC 和 Protobuf（与 Worker 一致）
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf REQUIRED)

# 指定 proto 库路径（假设 proto 构建在 ../proto/build）
set(PROTO_BUILD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../proto/build)
link_directories(${PROTO_BUILD_DIR})
include_directories(${PROTO_BUILD_DIR})

set(MANAGER_SOURCES
    src/main.cpp
    src/host_manager.cpp
    # src/query_manager.cpp
    src/rpc/grpc_server.cpp
    # src/rpc/query_service.cpp
)
add_executable(manager ${MANAGER_SOURCES})

target_link_libraries(manager
    monitor_proto
    gRPC::grpc++
    gRPC::grpc
    protobuf::libprotobuf
    pthread
    dl
)

# 添加头文件路径（proto 生成的头文件）

target_include_directories(manager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include   # 这是 Manager 自己的头文件
    ${PROTO_BUILD_DIR}                    # 这是 proto 生成的头文件
)