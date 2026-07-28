# # Manager - 管理者服务器
# # 部署在管理端，接收工作者推送的数据、存储到数据库、提供查询服务


# # MySQL 支持选项
# option(ENABLE_MYSQL "Enable MySQL support for data persistence" ON)

# set(MANAGER_SOURCES
#     src/main.cpp
#     src/host_manager.cpp
#     src/query_manager.cpp
#     src/rpc/grpc_server.cpp
#     src/rpc/query_service.cpp
# )

# # 可执行文件: manager (管理者服务器)
# add_executable(manager ${MANAGER_SOURCES})

# target_include_directories(manager PUBLIC
#     ${PROJECT_SOURCE_DIR}/manager/include
# )

# # 基础链接库
# set(MANAGER_LIBS monitor_proto gRPC::grpc++)

# # MySQL 依赖（可选）
# if(ENABLE_MYSQL)
#     find_package(PkgConfig QUIET)
#     if(PkgConfig_FOUND)
#         pkg_check_modules(MYSQL mysqlclient)
#     endif()
    
#     if(MYSQL_FOUND)
#         message(STATUS "MySQL support enabled")
#         target_compile_definitions(manager PRIVATE ENABLE_MYSQL=1)
#         target_include_directories(manager PUBLIC ${MYSQL_INCLUDE_DIRS})
#         list(APPEND MANAGER_LIBS ${MYSQL_LIBRARIES})
#     else()
#         message(WARNING "MySQL not found, disabling MySQL support")
#     endif()
# endif()

# target_link_libraries(manager PUBLIC ${MANAGER_LIBS})
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

add_executable(manager src/main.cpp)

target_link_libraries(manager
    monitor_proto
    gRPC::grpc++
    gRPC::grpc
    protobuf::libprotobuf
    pthread
    dl
)

# 添加头文件路径（proto 生成的头文件）
target_include_directories(manager PRIVATE ${PROTO_BUILD_DIR})