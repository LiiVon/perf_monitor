# ============================================
# Manager 子目录 CMakeLists.txt
# 被顶层 CMake 通过 add_subdirectory(manager) 调用
# 仅在 BUILD_MANAGER=ON 时构建
# ============================================

# 不需要 project() 和 find_package

# ===== MySQL 支持 =====
option(ENABLE_MYSQL "Enable MySQL support for data persistence" ON)

# 源文件列表
set(MANAGER_SOURCES
    src/main.cpp
    src/host_manager.cpp
    src/query_manager.cpp
    src/rpc/grpc_server.cpp
    src/rpc/query_service.cpp
)

# 可执行文件
add_executable(manager ${MANAGER_SOURCES})

# 头文件路径
target_include_directories(manager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include          # manager 自己的头文件
    ${CMAKE_CURRENT_SOURCE_DIR}/../proto/build   # proto 生成的头文件
)

# 基础链接库
set(MANAGER_LIBS
    monitor_proto
    gRPC::grpc++
    gRPC::grpc
    protobuf::libprotobuf
    pthread
    dl
)

# ===== MySQL 依赖（可选） =====
if(ENABLE_MYSQL)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(MYSQL mysqlclient)
    endif()

    if(MYSQL_FOUND)
        message(STATUS "MySQL support enabled")
        target_compile_definitions(manager PRIVATE ENABLE_MYSQL=1)
        target_include_directories(manager PUBLIC ${MYSQL_INCLUDE_DIRS})
        list(APPEND MANAGER_LIBS ${MYSQL_LIBRARIES})
    else()
        message(WARNING "MySQL not found, disabling MySQL support")
    endif()
endif()

target_link_libraries(manager PUBLIC ${MANAGER_LIBS})

message(STATUS "Manager configured: manager")